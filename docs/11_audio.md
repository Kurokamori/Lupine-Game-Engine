# 11 — Audio

`AudioManager` singleton (miniaudio) with a bus mixer + per-bus DSP effect chains, plus `AudioPlayer`/`AudioListener` components for spatialized playback. Script API on `Lupine`/`lupine`; C-API under `capi/include/audio/`. Sources: `core/include/lupine/audio/AudioManager.hpp`.

## Buses

Buses (e.g. `Master`, `Music`, `SFX`) are configured in the project (`audio.buses_state`) and the editor Audio Mixer panel. Routing: child bus → effect chain → parent bus → `Master`.

```lua
Lupine.set_bus_volume("Music", 0.6)     -- linear 0..1
Lupine.get_bus_volume("Music")
Lupine.set_bus_muted("SFX", true)
Lupine.get_bus_level("Master")          -- metering (current output level)
```

## Playback

```lua
local id = Lupine.play_audio("res://sfx/hit.wav", "SFX", false, 1.0)  -- path, bus, loop, volume
Lupine.play_audio_3d("res://sfx/engine.wav", {x=px,y=py,z=pz}, "SFX", true, 1.0)
Lupine.stop_audio(id); Lupine.pause_audio(id); Lupine.resume_audio(id)
```
3D playback attenuates relative to the active `AudioListener`. Scheduling, streaming, and a `finished` notification are available (see `04_scripting.md` audio scheduling).

## Components

- `AudioPlayer` — plays a clip from a node; 2D or 3D positional. Properties: stream/clip path, bus, autoplay, loop, volume, pitch, 3D attenuation.
- `AudioListener` — defines the listener pose for 3D mixing (usually on the camera/player).

## Bus DSP effects

Add effects to a bus chain and tune parameters live:
```lua
Lupine.add_bus_effect("Music", "reverb")
Lupine.set_bus_effect_parameter("Music", 0, "wet", 0.3)   -- bus, effect index, param, value
```
Effect types include biquad EQ/filter, reverb (Freeverb), delay, compressor, distortion, and chorus. Effects are backend-agnostic (`AudioDSP` + per-bus custom node). Parameter names depend on the effect type.

## Settings

`audio` section of `.lupine`: `master_volume`, `music_volume`, `sfx_volume`, `buses_state` (serialized mixer graph). Edit the mixer/effect chains in the editor's Audio Mixer panel.
