# RedMuck2 Sim (VST3)

Unofficial emulation of a Big Muff style fuzz with a front-end boost and switchable clipping, in the style of the
JAM Pedals Red Muck mk.2. It is a behavioural model built from the few published facts about the pedal,
**not** a schematic-exact copy, and is not affiliated with any manufacturer.

Published facts used: Muff-style fuzz inspired by the '71 "triangle" Big Muff and the later Sovtek "Civil War" Muff;
controls Gain, Tone, Level; a Boost footswitch driving a dedicated section tuned to hit the front end, with an
internal trimmer for its level; a 2-way switch for symmetrical or asymmetrical "+" clipping. Component values are
not published, so they are informed approximations taken from the well-known Muff topology.

## Signal flow
```
in -> Boost section -> input booster -> Gain -> clipping stage 1 -> clipping stage 2
   -> tone stack (low-pass / high-pass blend + fixed mid fill) -> recovery + Level -> out
```
Each clipping stage models Rf (100k) || Cf (about 500 pF) || diodes in the feedback path of a gain stage, solved per
sample. Asymmetrical mode puts one diode on one polarity and two in series on the other (my interpretation of "+").

## Controls
| Control | What it does |
|---|---|
| Gain | Level driven into the clipping stages (about -30 dB to 0 dB, log taper) |
| Tone | Left = bass (low-pass), right = treble (high-pass), centre = scooped mids (with a fixed mid fill) |
| Level | Output level (centre is about unity) |
| Boost Level | The internal trimmer of the real pedal: 0 = no boost, 1 = +24 dB |
| Boost (footswitch) | Engages the boost section in front of the fuzz |
| Clipping | Symmetrical, or Asymmetrical + (adds even harmonics, level matched) |
| ON / OFF | Footswitch with click-free crossfade |
| Input / Output | Host-side trims, -24 to +24 dB and -24 to +12 dB |

Runs at 4x oversampling (the plugin reports the oversampler latency to the host).

## Build
CMake 3.22+ and a C++17 compiler. JUCE 8.0.15 is downloaded automatically.
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Result: `build/RedMuck2Sim_artefacts/Release/VST3/RedMuck2 Sim.vst3`

Install: Windows `C:\Program Files\Common Files\VST3\`, macOS `~/Library/Audio/Plug-Ins/VST3/`, Linux `~/.vst3/`.

No compiler? Push this folder to GitHub: `.github/workflows/build.yml` builds Windows, macOS and Linux VST3s as downloadable artifacts.

Offline test: `cmake -S . -B build -DRM_BUILD_TESTS=ON && cmake --build build --target dsp_test && ./build/dsp_test`

JUCE is used under AGPLv3/commercial licensing: if you distribute the plugin, AGPL terms apply.
