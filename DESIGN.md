# Design

## Source of truth

- Status: Active
- Last refreshed: 2026-08-08
- Primary product surfaces: macOS Standalone, VST3 editor, AUv2 editor
- Evidence reviewed: `README.md`, `docs/02 Plugin Suite Survey and Implementation Plan.md`, `source/DigitalNoiseEditor.*`, current Standalone screenshot and user feedback

## Brand

- Personality: hostile computation, exposed machinery, forensic rather than nostalgic
- Trust signals: deterministic controls, visible values, stable gain ceiling, reproducible presets
- Avoid: random glitch overlays, fake terminal text, cyberpunk decoration unrelated to DSP, unreadable labels

## Product goals

- Goals: make heterogeneous binary structure and decoder mistakes audible; support excessive but controllable sound; keep every structural control legible
- Non-goals: imitate a named artist's undisclosed process; use white noise plus distortion as the primary identity; make safety controls ambiguous
- Success signals: headers, directories, alignment gaps, fixed-stride records, pointers, and payload boundaries create repeatable byte-pattern rhythms; raw-data modes produce abrupt format changes; no crashes or non-finite output

## Personas and jobs

- Primary personas: experimental electronic musicians, sound designers, noise performers
- User jobs: find extreme source material quickly; reproduce a machine state; automate transitions between data interpretations
- Key contexts of use: loud monitoring, DAW automation, standalone improvisation, rapid preset mutation

## Information architecture

- Primary navigation: one-page instrument panel
- Core routes/screens: parameter grid only
- Content hierarchy: source/clock and topology first, memory/address second, raw decoder controls next, output and seed last

## Design principles

- Expose the mechanism: labels and values name actual state operations.
- Excess through structure: density, discontinuity, and incompatible decoders are preferred over ornamental clutter.
- Preserve a safe exit: output remains bounded and all extreme modes are deterministic.
- Tradeoffs: information density may be high, but control hit areas and value readability remain intact.

## Visual language

- Color: near-black field, cyan active state, neutral gray inactive state; reserve toxic orange/magenta for decoder discontinuities or warnings
- Typography: compact system sans for controls; numeric values remain high contrast
- Spacing/layout rhythm: equal square rotary bounds in a fixed-ratio two-row grid
- Shape/radius/elevation: hard rectangles and circular controls; minimal shadows
- Motion: state-driven and bounded; no decorative random flicker
- Imagery/iconography: bit fields, addresses, record boundaries, and decoder state only when backed by DSP state

## Components

- Existing components to reuse: YUP `Slider`, `Label`, `AudioProcessorEditor`
- New/changed components: raw misread amount and format-smash controls; optional future DSP-state visualization
- Variants and states: default graph synthesis, blended raw decode, full raw decode, abrupt format boundary
- Token/component ownership: editor-local constants until YUP exposes a stable theme/token workflow

## Accessibility

- Target standard: practical desktop accessibility within current YUP capabilities
- Keyboard/focus behavior: host/YUP defaults; no hidden pointer-only mode switches
- Contrast/readability: labels and numeric values remain readable against the dark field
- Screen-reader semantics: constrained by current YUP accessibility support; control names must remain explicit
- Reduced motion and sensory considerations: no full-screen flashes; future animation must be state-driven and disableable

## Responsive behavior

- Supported breakpoints/devices: desktop plugin windows and macOS Standalone
- Layout adaptations: fixed aspect ratio; parameter count determines an even two-row grid
- Touch/hover differences: rotary vertical drag remains the primary interaction; hover is nonessential

## Interaction states

- Loading: immediate deterministic initialization
- Empty: not applicable; the instrument is self-running
- Error: invalid/non-finite parameter values clamp safely
- Success: parameter values update visibly and audio changes deterministically
- Disabled: no hidden disabled controls
- Offline/slow network: no runtime network dependency

## Content voice

- Tone: technical, terse, confrontational without parody
- Terminology: use real operations such as raw misread, byte order, address scramble, memory feedback
- Microcopy rules: short noun phrases; never imply historical provenance for artist/label references

## Implementation constraints

- Framework/styling system: C++20 and YUP GUI/audio processor modules
- Design-token constraints: current YUP slider styling is theme-owned; `Slider` textbox creation is incomplete, so values use separate labels
- Performance constraints: no allocation, file access, locks, or non-deterministic calls on the audio thread
- Compatibility constraints: macOS arm64 currently verified; state version changes require backward-compatible migration
- Test/screenshot expectations: engine regression tests, three-format build/signature checks, Standalone launch and screenshot inspection

## Open questions

- [ ] Should a later version load user-selected files off the audio thread, or keep the virtual structured blob fully deterministic?
- [ ] Should raw decoder mode names replace numeric values when YUP enum formatting is stable?
- [ ] Which DSP state signals are safe and useful to expose visually without audio-thread synchronization hazards?
