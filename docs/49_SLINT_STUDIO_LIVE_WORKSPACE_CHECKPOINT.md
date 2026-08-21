# Slint Studio / Live workspace checkpoint

Status: accepted Preview 111 composition pass for the unsigned Windows
testing-preview line.

## Product outcome

The default Slint shell now has two explicit top-level workspaces:

- **Studio — Build and rehearse** keeps the accepted Fixtures + Static Looks
  authoring journey, output-free simulation, and explicitly armed bounded
  fixture preview.
- **Live — Perform the show** gives the existing Runner, Autoloop, and saved
  Static Look controls a dedicated performance canvas.

The Live health/safety strip stays pinned across both workspaces. It continues
to expose Runner state, sync/BPM, output health, active content, Start/Stop,
Blackout, Work Light, Release Overrides, and saved-Look ownership through the
canonical model and `UiCommandFacade`.

## Live composition

The full selected Autoloop bank is rendered as a 4×8 matrix sourced from the
existing 32 `LiveViewModel` pad projections. Selected, actively playing,
disabled-by-filter, and empty pads remain distinct. Active playback carries
authoritative progress. The surrounding controls retain the existing 64-bank
paging, four-bank window, all-bank/selected-bank filtering, launch,
previous/next, and clear commands.

Saved Static Looks share the Live canvas so an operator can select and take or
release a sparse override without leaving performance context. Released
properties continue to reveal the advancing Track Script or Autoloop beneath
them; the UI does not create a second ownership model.

## Reference and reuse audit

Public SoundSwitch documentation was used to verify the importance of visible
bank selection, active-bank feedback, exclusive bank filtering, and dense pad
access. QLC+ Virtual Console documentation and its Apache-2.0 repository were
reviewed for model-driven page, frame, button-matrix, and panic/control-strip
patterns. Slint's reusable components and layout primitives remain the
implementation mechanism.

No SoundSwitch code, proprietary assets, or trade dress were copied. No QLC+
source was copied and no Qt/QML dependency was added. This keeps the pass
inside EmberLights' existing Slint shell, registry, domain, Runner, output,
safety, project, and migration boundaries. `THIRD_PARTY_NOTICES.md` remains the
authority for any dependency that actually ships.

## Gate and claim boundary

Preview 111 must continue to pass the exact Slint 1.17.1 syntax gate, native
tests, generated registry check, warning-fatal Windows x64 cross-build, package
contract, PE/runtime-closure inspection, installed-layout smoke simulations,
and deterministic installer construction.

Those non-Windows gates do not prove native installed-Windows behavior,
GPU/rendering compatibility, Windows DPI, keyboard navigation, Narrator or
accessibility behavior, physical fixture response, Authenticode, soak, or gig
readiness. Physical output remains disabled by default and self-tests remain
non-outputting.
