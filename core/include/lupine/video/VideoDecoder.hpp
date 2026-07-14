#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace lupine {
namespace video {

/**
 * A single decoded video frame.
 *
 * Pixels are RGBA8, tightly packed, and pre-flipped vertically to match the
 * engine's texture convention (same as ImageAsset), so a frame can be uploaded
 * straight to a GPU texture and rendered through drawSprite on any backend.
 */
struct VideoFrameRGBA {
    std::vector<uint8_t> pixels;  // width*height*4 bytes, RGBA8
    int width{0};
    int height{0};
    double pts{0.0};              // presentation timestamp in seconds
};

/**
 * The fully decoded audio track of a video, interleaved signed 16-bit PCM,
 * resampled to a fixed sample rate / channel count so it can be played back
 * through the engine's AudioManager (which expects s16 interleaved data).
 */
struct VideoAudioPCM {
    std::vector<uint8_t> data;    // interleaved S16
    uint32_t sampleRate{0};
    uint32_t channels{0};

    bool IsValid() const { return !data.empty() && sampleRate > 0 && channels > 0; }
};

/**
 * Video decoder backed by FFmpeg (avformat/avcodec/swscale/swresample).
 *
 * The header is intentionally free of any FFmpeg type so that components can use
 * it identically whether or not the engine was built with LUPINE_ENABLE_VIDEO.
 * When the FFmpeg backend is not compiled in, every method degrades gracefully
 * (Open() returns false, IsBackendAvailable() returns false).
 *
 * Video frame stepping and full audio extraction use independent decode
 * contexts, so decoding the audio track never disturbs frame-by-frame playback.
 */
class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;

    /** True if the engine was compiled with the FFmpeg video backend. */
    static bool IsBackendAvailable();

    /**
     * Open a video from a res:// path, physical path, or pack asset.
     * @return true on success; false on failure or when the backend is absent.
     */
    bool Open(const std::string& path);

    /** Release all decoder resources. Safe to call multiple times. */
    void Close();

    bool IsOpen() const;

    int GetWidth() const;
    int GetHeight() const;
    double GetDuration() const;   // seconds; 0 if unknown
    double GetFrameRate() const;  // frames per second; 0 if unknown
    bool HasAudio() const;

    /**
     * Decode the next video frame into `out`.
     * @return true on success, false at end-of-stream or on error.
     */
    bool DecodeNextFrame(VideoFrameRGBA& out);

    /**
     * Rewind to the start of the stream (for looping playback).
     * @return true on success.
     */
    bool Rewind();

    /**
     * Seek to (approximately) the given time in seconds. Seeks to the nearest
     * preceding keyframe and flushes the decoder, so the next DecodeNextFrame()
     * returns a frame at or after `seconds`. Clamped to [0, duration].
     * @return true on success.
     */
    bool Seek(double seconds);

    /**
     * Decode the entire audio track to interleaved S16 PCM, resampled to
     * `targetSampleRate` / `targetChannels`. Returns false if the file has no
     * audio stream or the backend is unavailable.
     */
    bool DecodeAudioPCM(uint32_t targetSampleRate, uint32_t targetChannels, VideoAudioPCM& out);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace video
} // namespace lupine
