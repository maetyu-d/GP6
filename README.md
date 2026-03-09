# GP6

GP-6 is a JUCE-built Audio Unit inspired by the COMDYNA GP-6 analog computer, reimagined as an expanded software instrument with a patchable front panel, live cable routing, and a larger simulated signal network than the original hardware. It combines analog-computing ideas like integrators, summers, multipliers, comparators, overload behavior, and reset logic with a performance-oriented patch view that provides twelve interactive cable routes, animated patch states, and presets that make use of the expanded architecture. 

![](https://github.com/maetyu-d/GP6/blob/main/Screenshot%202026-03-09%20at%2015.38.47.png)
![](https://github.com/maetyu-d/GP6/blob/main/Screenshot%202026-03-09%20at%2015.38.31.png)

The included AU bundle is packaged as GP6-AU.component.zip for direct installation on macOS.

## Features

- AU MIDI FX target for Logic Pro, with `VST3` and `Standalone` targets also generated for development.
- Preset bank with varied generative starting points.

## Build

You need a local JUCE checkout.

```bash
cmake -B build -S . -DJUCE_DIR=/absolute/path/to/JUCE
cmake --build build --config Release
```

For Logic Pro, use the generated AU target. The plugin is configured as a MIDI effect (`IS_MIDI_EFFECT TRUE`) so it can be inserted in Logic's MIDI FX slot.

## Notes

- The processor depends on host transport and tempo information for sample-accurate scheduling.
- Presets are designed as performance starting points. You can load a preset variation, then retune the output behavior to match any synth, drum rack or sampler.
