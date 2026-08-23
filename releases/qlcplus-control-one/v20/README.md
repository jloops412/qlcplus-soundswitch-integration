# V20 Staged Release Candidate

Files:

- `IR4-TUBES-CONTROL-ONE-V20-UIUX-PORTABLE.qxw` — latest QLC+ workspace/project;
- `SoundSwitch-Control-One-Performance.qxi` — matching Control One input profile;
- `soundswitch.dll` — matching DJ-PC plug-in binary;
- `SHA256SUMS.txt` — integrity hashes.

This is a staged backup and test set, not a universal installer. The workspace's Micro serial-specific UID was cleared for privacy and portability. After opening it, select the desired output in QLC+ Input/Output and save a machine-local copy.

The binary is known to work in the current DJ-PC QLC+ installation, whose profile identifies `5.3.0 GIT a124abe`. It has not been qualified as ABI-compatible with other QLC+/Qt builds. Keep the previous known-good application folder and workspace for rollback.

See `docs/qlcplus-control-one/V20_RELEASE_NOTES.md` and `PROJECT_STATUS_AND_ROADMAP.md` before testing.
