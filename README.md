# erhcetua

`erhcetua` is a JUCE MIDI FX plugin for Logic Pro focused on reset-driven generative grammar, lane mutation, and Autechre-inspired rhythm generation.

## Features

- AU MIDI FX target for Logic Pro, with `VST3` and `Standalone` targets also generated for development.
- Reset-driven rhythm generation with grammar, mutation, flutter, rotation, drift, and conditional rules.
- Multiple internal lanes with per-step modulation rows for velocity, gate, octave, and reset behavior.
- Scale-aware pitch generation and legato mode for connected phrases.
- Compact visual editor that exposes the live algorithm stages and step field directly.
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
- Presets are designed as performance starting points. You can load a preset variation, then retune the output behavior to match any drum rack or sampler.
