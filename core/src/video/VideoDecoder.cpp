#include "lupine/video/VideoDecoder.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/logger/Logger.hpp"

#include <algorithm>
#include <cstring>

#ifdef LUPINE_ENABLE_VIDEO
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#endif

namespace lupine {
namespace video {

#ifdef LUPINE_ENABLE_VIDEO

namespace {

// Resolve an asset path to a physical path, falling back to the original.
std::string ResolvePhysicalPath(const std::string& path) {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    auto& assetDb = asset::AssetDatabase::GetInstance();
    if (assetDb.IsInitialized()) {
        std::string r = assetDb.ResolveAsset(normalized);
        if (!r.empty()) {
            return r;
        }
    }
    return normalized;
}

// Read every byte of a pack asset. Returns false if not in pack mode / not found.
bool ReadPackBytes(const std::string& path, std::vector<uint8_t>& out) {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    auto& packFS = platform::PackFileSystem::Instance();
    if (!packFS.isPackMode()) {
        return false;
    }
    std::string packPath = packFS.resolveAsset(normalized);
    if (!packFS.exists(packPath)) {
        return false;
    }
    out = packFS.readAsset(normalized);
    return !out.empty();
}

// In-memory AVIO source, used when a video is served from a mounted pack file.
struct MemorySource {
    std::vector<uint8_t> bytes;
    size_t pos{0};
};

int MemoryRead(void* opaque, uint8_t* buf, int bufSize) {
    MemorySource* src = static_cast<MemorySource*>(opaque);
    if (src->pos >= src->bytes.size()) {
        return AVERROR_EOF;
    }
    size_t remaining = src->bytes.size() - src->pos;
    size_t toRead = std::min(static_cast<size_t>(bufSize), remaining);
    std::memcpy(buf, src->bytes.data() + src->pos, toRead);
    src->pos += toRead;
    return static_cast<int>(toRead);
}

int64_t MemorySeek(void* opaque, int64_t offset, int whence) {
    MemorySource* src = static_cast<MemorySource*>(opaque);
    const int64_t size = static_cast<int64_t>(src->bytes.size());
    if (whence & AVSEEK_SIZE) {
        return size;
    }
    int64_t newPos;
    switch (whence & ~AVSEEK_FORCE) {
        case SEEK_SET: newPos = offset; break;
        case SEEK_CUR: newPos = static_cast<int64_t>(src->pos) + offset; break;
        case SEEK_END: newPos = size + offset; break;
        default: return -1;
    }
    if (newPos < 0 || newPos > size) {
        return -1;
    }
    src->pos = static_cast<size_t>(newPos);
    return newPos;
}

// Holds an opened demuxer plus the optional in-memory source backing it.
struct OpenedFormat {
    AVFormatContext* fmt{nullptr};
    AVIOContext* avio{nullptr};
    MemorySource* memory{nullptr};

    void Destroy() {
        if (fmt) {
            avformat_close_input(&fmt);
            fmt = nullptr;
        }
        if (avio) {
            // The internal buffer may have been reallocated by FFmpeg.
            av_freep(&avio->buffer);
            avio_context_free(&avio);
            avio = nullptr;
        }
        delete memory;
        memory = nullptr;
    }
};

// Open a demuxer for the given asset path. Uses an in-memory AVIO when the asset
// lives in a mounted pack, otherwise opens the resolved physical path directly
// (which lets FFmpeg stream large files from disk).
bool OpenFormat(const std::string& path, OpenedFormat& out) {
    std::vector<uint8_t> packBytes;
    if (ReadPackBytes(path, packBytes)) {
        out.memory = new MemorySource();
        out.memory->bytes = std::move(packBytes);

        const int bufferSize = 1 << 16;
        unsigned char* ioBuffer = static_cast<unsigned char*>(av_malloc(bufferSize));
        if (!ioBuffer) {
            out.Destroy();
            return false;
        }
        out.avio = avio_alloc_context(ioBuffer, bufferSize, 0, out.memory,
                                      &MemoryRead, nullptr, &MemorySeek);
        if (!out.avio) {
            av_freep(&ioBuffer);
            out.Destroy();
            return false;
        }

        out.fmt = avformat_alloc_context();
        if (!out.fmt) {
            out.Destroy();
            return false;
        }
        out.fmt->pb = out.avio;
        out.fmt->flags |= AVFMT_FLAG_CUSTOM_IO;

        if (avformat_open_input(&out.fmt, nullptr, nullptr, nullptr) < 0) {
            out.fmt = nullptr;  // avformat_open_input frees on failure
            out.Destroy();
            return false;
        }
    } else {
        std::string physical = ResolvePhysicalPath(path);
        if (avformat_open_input(&out.fmt, physical.c_str(), nullptr, nullptr) < 0) {
            out.fmt = nullptr;
            out.Destroy();
            return false;
        }
    }

    if (avformat_find_stream_info(out.fmt, nullptr) < 0) {
        out.Destroy();
        return false;
    }
    return true;
}

} // namespace

struct VideoDecoder::Impl {
    std::string sourcePath;

    OpenedFormat format;
    int videoStreamIndex{-1};
    AVCodecContext* codecCtx{nullptr};
    SwsContext* sws{nullptr};
    AVPacket* packet{nullptr};
    AVFrame* frame{nullptr};

    int width{0};
    int height{0};
    double duration{0.0};
    double frameRate{0.0};
    bool hasAudio{false};
    bool flushSent{false};

    ~Impl() { Cleanup(); }

    void Cleanup() {
        if (sws) {
            sws_freeContext(sws);
            sws = nullptr;
        }
        if (frame) {
            av_frame_free(&frame);
        }
        if (packet) {
            av_packet_free(&packet);
        }
        if (codecCtx) {
            avcodec_free_context(&codecCtx);
        }
        format.Destroy();
        videoStreamIndex = -1;
        flushSent = false;
    }

    bool ConvertFrame(VideoFrameRGBA& out) {
        const int w = frame->width;
        const int h = frame->height;
        if (w <= 0 || h <= 0) {
            return false;
        }

        sws = sws_getCachedContext(sws,
                                   w, h, static_cast<AVPixelFormat>(frame->format),
                                   w, h, AV_PIX_FMT_RGBA,
                                   SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!sws) {
            return false;
        }

        out.width = w;
        out.height = h;
        out.pixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);

        // Write rows bottom-up (negative stride, last row first) so the result is
        // vertically flipped to match the engine's texture convention.
        const int dstStride = w * 4;
        uint8_t* dst[4] = { out.pixels.data() + static_cast<size_t>(h - 1) * dstStride,
                            nullptr, nullptr, nullptr };
        int dstStrides[4] = { -dstStride, 0, 0, 0 };

        sws_scale(sws, frame->data, frame->linesize, 0, h, dst, dstStrides);

        int64_t ts = frame->best_effort_timestamp;
        if (ts == AV_NOPTS_VALUE) {
            ts = frame->pts;
        }
        if (ts == AV_NOPTS_VALUE) {
            out.pts = 0.0;
        } else {
            AVRational tb = format.fmt->streams[videoStreamIndex]->time_base;
            out.pts = static_cast<double>(ts) * av_q2d(tb);
        }
        return true;
    }
};

VideoDecoder::VideoDecoder() : m_Impl(std::make_unique<Impl>()) {}
VideoDecoder::~VideoDecoder() = default;

bool VideoDecoder::IsBackendAvailable() { return true; }

bool VideoDecoder::Open(const std::string& path) {
    Close();
    m_Impl = std::make_unique<Impl>();
    Impl& d = *m_Impl;
    d.sourcePath = path;

    if (!OpenFormat(path, d.format)) {
        LOG_ERROR(LogCategory::Core, "VideoDecoder: failed to open {}", path);
        d.Cleanup();
        return false;
    }

    const AVCodec* decoder = nullptr;
    d.videoStreamIndex = av_find_best_stream(d.format.fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (d.videoStreamIndex < 0 || !decoder) {
        LOG_ERROR(LogCategory::Core, "VideoDecoder: no video stream in {}", path);
        d.Cleanup();
        return false;
    }

    AVStream* stream = d.format.fmt->streams[d.videoStreamIndex];

    d.codecCtx = avcodec_alloc_context3(decoder);
    if (!d.codecCtx) {
        d.Cleanup();
        return false;
    }
    if (avcodec_parameters_to_context(d.codecCtx, stream->codecpar) < 0) {
        d.Cleanup();
        return false;
    }
    if (avcodec_open2(d.codecCtx, decoder, nullptr) < 0) {
        LOG_ERROR(LogCategory::Core, "VideoDecoder: failed to open codec for {}", path);
        d.Cleanup();
        return false;
    }

    d.width = d.codecCtx->width;
    d.height = d.codecCtx->height;

    if (d.format.fmt->duration != AV_NOPTS_VALUE && d.format.fmt->duration > 0) {
        d.duration = static_cast<double>(d.format.fmt->duration) / AV_TIME_BASE;
    } else if (stream->duration != AV_NOPTS_VALUE && stream->duration > 0) {
        d.duration = static_cast<double>(stream->duration) * av_q2d(stream->time_base);
    }

    AVRational fr = stream->avg_frame_rate;
    if (fr.num == 0 || fr.den == 0) {
        fr = stream->r_frame_rate;
    }
    if (fr.num != 0 && fr.den != 0) {
        d.frameRate = av_q2d(fr);
    }

    d.hasAudio = av_find_best_stream(d.format.fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0) >= 0;

    d.packet = av_packet_alloc();
    d.frame = av_frame_alloc();
    if (!d.packet || !d.frame) {
        d.Cleanup();
        return false;
    }

    return true;
}

void VideoDecoder::Close() {
    if (m_Impl) {
        m_Impl->Cleanup();
    }
}

bool VideoDecoder::IsOpen() const {
    return m_Impl && m_Impl->codecCtx != nullptr && m_Impl->videoStreamIndex >= 0;
}

int VideoDecoder::GetWidth() const { return m_Impl ? m_Impl->width : 0; }
int VideoDecoder::GetHeight() const { return m_Impl ? m_Impl->height : 0; }
double VideoDecoder::GetDuration() const { return m_Impl ? m_Impl->duration : 0.0; }
double VideoDecoder::GetFrameRate() const { return m_Impl ? m_Impl->frameRate : 0.0; }
bool VideoDecoder::HasAudio() const { return m_Impl ? m_Impl->hasAudio : false; }

bool VideoDecoder::DecodeNextFrame(VideoFrameRGBA& out) {
    if (!IsOpen()) {
        return false;
    }
    Impl& d = *m_Impl;

    for (;;) {
        int ret = avcodec_receive_frame(d.codecCtx, d.frame);
        if (ret == 0) {
            bool ok = d.ConvertFrame(out);
            av_frame_unref(d.frame);
            return ok;
        }
        if (ret == AVERROR_EOF) {
            return false;
        }
        if (ret != AVERROR(EAGAIN)) {
            return false;
        }

        // Decoder needs more input: feed it the next video packet.
        bool fed = false;
        while (!fed) {
            int rret = av_read_frame(d.format.fmt, d.packet);
            if (rret < 0) {
                if (!d.flushSent) {
                    avcodec_send_packet(d.codecCtx, nullptr);  // enter draining mode
                    d.flushSent = true;
                }
                fed = true;  // go back to receive (will drain remaining frames)
                break;
            }
            if (d.packet->stream_index == d.videoStreamIndex) {
                int sret = avcodec_send_packet(d.codecCtx, d.packet);
                av_packet_unref(d.packet);
                if (sret < 0 && sret != AVERROR(EAGAIN)) {
                    return false;
                }
                fed = true;
            } else {
                av_packet_unref(d.packet);
            }
        }
    }
}

bool VideoDecoder::Rewind() {
    if (!IsOpen()) {
        return false;
    }
    Impl& d = *m_Impl;
    int ret = av_seek_frame(d.format.fmt, d.videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        // Fall back to a generic seek to timestamp 0.
        ret = avformat_seek_file(d.format.fmt, -1, INT64_MIN, 0, INT64_MAX, 0);
    }
    avcodec_flush_buffers(d.codecCtx);
    d.flushSent = false;
    return ret >= 0;
}

bool VideoDecoder::Seek(double seconds) {
    if (!IsOpen()) {
        return false;
    }
    Impl& d = *m_Impl;

    if (seconds < 0.0) {
        seconds = 0.0;
    }
    if (d.duration > 0.0 && seconds > d.duration) {
        seconds = d.duration;
    }

    AVRational tb = d.format.fmt->streams[d.videoStreamIndex]->time_base;
    int64_t ts = static_cast<int64_t>(seconds / av_q2d(tb));
    int ret = av_seek_frame(d.format.fmt, d.videoStreamIndex, ts, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        int64_t tsAv = static_cast<int64_t>(seconds * AV_TIME_BASE);
        ret = avformat_seek_file(d.format.fmt, -1, INT64_MIN, tsAv, INT64_MAX, 0);
    }
    avcodec_flush_buffers(d.codecCtx);
    d.flushSent = false;
    return ret >= 0;
}

bool VideoDecoder::DecodeAudioPCM(uint32_t targetSampleRate, uint32_t targetChannels, VideoAudioPCM& out) {
    out = VideoAudioPCM();
    if (!m_Impl || targetSampleRate == 0 || targetChannels == 0) {
        return false;
    }

    OpenedFormat fmt;
    if (!OpenFormat(m_Impl->sourcePath, fmt)) {
        return false;
    }

    const AVCodec* decoder = nullptr;
    int audioStreamIndex = av_find_best_stream(fmt.fmt, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
    if (audioStreamIndex < 0 || !decoder) {
        fmt.Destroy();
        return false;
    }

    AVStream* stream = fmt.fmt->streams[audioStreamIndex];
    AVCodecContext* codecCtx = avcodec_alloc_context3(decoder);
    if (!codecCtx) {
        fmt.Destroy();
        return false;
    }
    if (avcodec_parameters_to_context(codecCtx, stream->codecpar) < 0 ||
        avcodec_open2(codecCtx, decoder, nullptr) < 0) {
        avcodec_free_context(&codecCtx);
        fmt.Destroy();
        return false;
    }

    // Ensure the source channel layout is valid (some decoders leave it unset).
    AVChannelLayout inLayout;
    av_channel_layout_default(&inLayout, codecCtx->ch_layout.nb_channels > 0
                                             ? codecCtx->ch_layout.nb_channels
                                             : 2);
    if (codecCtx->ch_layout.nb_channels > 0) {
        av_channel_layout_copy(&inLayout, &codecCtx->ch_layout);
    }

    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, static_cast<int>(targetChannels));

    SwrContext* swr = nullptr;
    int swrRet = swr_alloc_set_opts2(&swr,
                                     &outLayout, AV_SAMPLE_FMT_S16, static_cast<int>(targetSampleRate),
                                     &inLayout, codecCtx->sample_fmt, codecCtx->sample_rate,
                                     0, nullptr);
    if (swrRet < 0 || !swr || swr_init(swr) < 0) {
        if (swr) swr_free(&swr);
        av_channel_layout_uninit(&inLayout);
        av_channel_layout_uninit(&outLayout);
        avcodec_free_context(&codecCtx);
        fmt.Destroy();
        return false;
    }

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    bool ok = (packet && frame);

    auto appendConverted = [&](const uint8_t** inData, int inSamples) {
        int maxOut = static_cast<int>(av_rescale_rnd(
            swr_get_delay(swr, codecCtx->sample_rate) + inSamples,
            static_cast<int>(targetSampleRate), codecCtx->sample_rate, AV_ROUND_UP));
        if (maxOut <= 0) {
            return;
        }
        uint8_t* outBuf = nullptr;
        if (av_samples_alloc(&outBuf, nullptr, static_cast<int>(targetChannels),
                             maxOut, AV_SAMPLE_FMT_S16, 0) < 0) {
            return;
        }
        int converted = swr_convert(swr, &outBuf, maxOut, inData, inSamples);
        if (converted > 0) {
            size_t bytes = static_cast<size_t>(converted) * targetChannels * sizeof(int16_t);
            out.data.insert(out.data.end(), outBuf, outBuf + bytes);
        }
        av_freep(&outBuf);
    };

    if (ok) {
        while (av_read_frame(fmt.fmt, packet) >= 0) {
            if (packet->stream_index == audioStreamIndex) {
                if (avcodec_send_packet(codecCtx, packet) >= 0) {
                    while (avcodec_receive_frame(codecCtx, frame) == 0) {
                        appendConverted(const_cast<const uint8_t**>(frame->extended_data),
                                        frame->nb_samples);
                        av_frame_unref(frame);
                    }
                }
            }
            av_packet_unref(packet);
        }

        // Flush the decoder.
        avcodec_send_packet(codecCtx, nullptr);
        while (avcodec_receive_frame(codecCtx, frame) == 0) {
            appendConverted(const_cast<const uint8_t**>(frame->extended_data), frame->nb_samples);
            av_frame_unref(frame);
        }

        // Flush any samples buffered inside the resampler.
        appendConverted(nullptr, 0);
    }

    if (frame) av_frame_free(&frame);
    if (packet) av_packet_free(&packet);
    swr_free(&swr);
    av_channel_layout_uninit(&inLayout);
    av_channel_layout_uninit(&outLayout);
    avcodec_free_context(&codecCtx);
    fmt.Destroy();

    if (!out.data.empty()) {
        out.sampleRate = targetSampleRate;
        out.channels = targetChannels;
    }
    return out.IsValid();
}

#else // LUPINE_ENABLE_VIDEO not defined: graceful no-op backend.

struct VideoDecoder::Impl {};

VideoDecoder::VideoDecoder() : m_Impl(nullptr) {}
VideoDecoder::~VideoDecoder() = default;

bool VideoDecoder::IsBackendAvailable() { return false; }

bool VideoDecoder::Open(const std::string& path) {
    LOG_WARN(LogCategory::Core,
             "VideoDecoder: engine built without LUPINE_ENABLE_VIDEO; cannot play {}", path);
    return false;
}

void VideoDecoder::Close() {}
bool VideoDecoder::IsOpen() const { return false; }
int VideoDecoder::GetWidth() const { return 0; }
int VideoDecoder::GetHeight() const { return 0; }
double VideoDecoder::GetDuration() const { return 0.0; }
double VideoDecoder::GetFrameRate() const { return 0.0; }
bool VideoDecoder::HasAudio() const { return false; }
bool VideoDecoder::DecodeNextFrame(VideoFrameRGBA&) { return false; }
bool VideoDecoder::Rewind() { return false; }
bool VideoDecoder::Seek(double) { return false; }
bool VideoDecoder::DecodeAudioPCM(uint32_t, uint32_t, VideoAudioPCM&) { return false; }

#endif // LUPINE_ENABLE_VIDEO

} // namespace video
} // namespace lupine
