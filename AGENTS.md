# DigitalNoise Workspace Contract

## Scope

- This repository owns the `DigitalNoise` product only: Standalone, VST3, and AUv2.
- Do not add unrelated plugin products, suites, engines, or generated bundles without an explicit user request.
- Treat `build/` as generated output, never as the source of truth.

## Preserved product contracts

- Keep the structured 8192-byte virtual binary image and raw decoder deterministic.
- Preserve existing host parameter IDs. `Output` and `Graph Seed` remain host IDs 8 and 9; `Raw Misread` and `Format Smash` use 10 and 11.
- Preserve v1-to-v2 state migration and finite output bounded to `±0.95`.
- Keep audio-thread processing free of allocation, locks, file I/O, and non-deterministic calls.

## Change protocol

- Inspect `git status` before editing and do not overwrite unrelated work.
- Keep source changes in Git and leave a clean, verified commit when the task requests completed implementation.
- Verify DSP changes with the assert-enabled `engine-debug` tests, then build and test the plugin-release targets.
- Store durable project knowledge under `yup/DigitalNoise/` through `obsidian-http`; store reusable YUP-specific pitfalls under `yup/`.
