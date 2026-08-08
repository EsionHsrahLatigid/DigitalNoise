# DigitalNoise

DigitalNoise is a self-running stereo noise instrument built with [YUP](https://github.com/kunitoki/yup). It generates sound from coupled state machines instead of using a white-noise oscillator as its source.

The current prototype builds as a standalone macOS app, VST3, and Audio Unit v2. Its compact editor exposes all twelve graph and raw-decoder parameters in both the app and supporting plugin hosts.

This repository intentionally owns the `DigitalNoise` product only. The adjacent YUP checkout is a dependency, and anything under `build/` is generated output rather than project source.

## Sound engine

The signal is produced by a deterministic graph of audible computations:

1. Two 32-bit Galois LFSRs advance pseudo-random but repeatable bit sequences.
2. Two ring-shaped 32-cell cellular automata evolve with morphed 8-bit rules.
3. A 16-step finite-state sequencer changes bit planes, rotations, and arithmetic operators.
4. A 2048-word ring memory is read through addresses derived from the current CA, LFSR, and graph state.
5. A contradictory virtual binary image is overlaid onto that memory: superblock, directory entries, one MP4-style box fragment, sparse alignment space, fixed-stride packets, a node tree, pointer table, entropy payload, and repeated footer markers.
6. The raw decoder deliberately misreads those changing structures as signed or unsigned 8-bit PCM, 16/24-bit PCM in either byte order, a one-bit serial stream, or wrapping delta data.
7. Unsigned wrap arithmetic, XOR, masks, and bit-plane folding turn those states directly into stereo samples.
8. A final hard safety bound keeps output finite and below `±0.95`; it is not the primary sound source.

No allocation, locking, file access, or random-device call occurs in the audio processing path. A graph seed reproduces exactly the same sequence. Seed automation is disabled: full graph resets are consumed once at a block boundary, while MIDI notes use a lightweight core-only reseed at their exact sample position.

## Parameters

| Parameter | Function |
| --- | --- |
| Machine Rate | Control-rate clock for graph transitions, 20–24000 Hz |
| CA Topology | Morphs the cellular-automaton rule family |
| Mutation | Exposes more changing CA/LFSR bit planes |
| Memory Depth | Sets the reachable history in the ring memory |
| Address Scramble | Rotates and cross-couples read addresses |
| Memory Feedback | Returns addressed words to the graph |
| Stereo Divergence | Separates the two coupled state branches |
| Bitplane Intensity | Raises the contribution of raw folded words |
| Raw Misread | Crossfades from the graph output into the virtual binary-file decoder |
| Format Smash | Crosses incompatible PCM, byte-order, bitstream, offset, and stride interpretations |
| Output | Final gain, -48 to +6 dB before the safety bound |
| Graph Seed | Selects a deterministic graph and memory state |

The instrument runs without MIDI. A MIDI note-on resets the graph to a note-derived state and transposes `Machine Rate` relative to C4, so repeated notes create hard structural cuts rather than a conventional oscillator envelope.

## Requirements

- macOS 11 or newer
- Apple Clang with C++20 support
- CMake 3.31 or newer
- Ninja
- Xcode / macOS SDK for AU builds
- A local YUP checkout at `../yup`, or network access for the pinned fallback checkout

YUP is pinned to commit `9a1c9bc699b6a714f6f52486462d98a140c8bf95` when the adjacent checkout is absent. YUP is ISC-licensed; its own license and all fetched dependency licenses remain authoritative.

## Build and test

Fast DSP-only loop:

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug
ctest --preset engine-debug
```

Release app and plugins:

```sh
cmake --preset plugin-release
cmake --build --preset plugin-release --parallel
ctest --preset plugin-release
```

The first plugin configure may download YUP's upstream sources, SDL3, Steinberg's VST3 SDK, and Apple's AudioUnitSDK.

Artifacts:

- `build/plugin-release/digitalnoise_standalone_plugin.app`
- `build/plugin-release/VST3/Release/digitalnoise_vst3_plugin.vst3`
- `build/plugin-release/digitalnoise_au_plugin.component`

Local installation is intentionally separate from the build:

```sh
cp -R build/plugin-release/VST3/Release/digitalnoise_vst3_plugin.vst3 "$HOME/Library/Audio/Plug-Ins/VST3/"
cp -R build/plugin-release/digitalnoise_au_plugin.component "$HOME/Library/Audio/Plug-Ins/Components/"
```

The local macOS build ad-hoc signs the standalone app and VST3 bundle. Distribution still requires your Developer ID signing and notarization workflow.

## Verification covered

The engine tests check the `DigitalNoise` DSP and structured raw-decoder invariants. Coverage includes:

- identical output for identical seed and parameters;
- divergence between different seeds;
- exact reset reproducibility;
- finite, bounded output at extreme parameter values;
- safe fallback for NaN and infinite parameter inputs;
- a clocked sample-and-hold signature that ordinary white noise would not satisfy;
- sequence changes caused by topology, memory-depth, and address parameters;
- exact looping of the 8192-byte raw-file image before graph mutation;
- audible superblock bytes, an embedded MP4-style box fragment, periodic packet headers, and divergent raw decoder formats;
- non-constant, non-silent structure;
- meaningful stereo divergence;

The current macOS build has also been checked for successful Standalone/VST3/AU linkage, valid arm64 Mach-O bundles, and valid local code signatures.

## Research boundary

Public sources support the use of computers, algorithmic generation, real-time tape/laptop processes, cellular automata, LFSRs, granular scheduling, and time-varying memory as relevant technical material. They generally do **not** disclose the exact algorithms used by Mego/eMego, Merzbow, Raster-Noton, Warp, or Staalplaat artists. This project therefore treats those names as aesthetic coordinates and labels its concrete DSP mapping as design inference, not historical provenance.

Durable research, source evaluation, architecture, and verification notes live in the Obsidian namespace `yup/DigitalNoise/`.

## Current limits

- The current editor is functional and intentionally minimal; it does not yet visualize the evolving graph state.
- No amplitude envelope; the instrument is intentionally self-running.
- No universal binary has been produced yet; the verified artifacts are arm64.
- Full DAW scanning, AU/VST3 host validation, listening, and calibrated loudness tests remain host-specific follow-up work.
