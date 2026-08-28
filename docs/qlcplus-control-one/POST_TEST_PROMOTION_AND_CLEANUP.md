# Post-Test Promotion and Local Cleanup

Do not run local machine cleanup until the V26 owner test passes. Cleanup should
make the current release obvious without destroying rollback evidence.

## Promotion rule

After V26 passes:

1. Close QLC+ so no workspace or plug-in is in use.
2. Keep one clearly named working copy:
   `IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw`.
3. Keep the complete V26 package and its `SHA256SUMS.txt` unchanged.
4. Keep V24 as the Runtime Feedback rollback, V23 as the Live Console rollback,
   V22 as the unified creative rollback, V21 as the reliability rollback, and
   V20 as the protected creative baseline.
5. Keep V25 locally as the reviewed source used to generate V26.
6. Keep the installer-created two-plug-in backup and receipt with the pinned
   QLC+ installation.
7. Move earlier experiments and duplicate numbered workspaces into one dated
   `Legacy-Pre-V26` archive folder.
8. Do not delete fixture definitions, VirtualDJ mappings, Control One profiles,
   QLC+ core/runtime files, or any file whose purpose has not been verified.

## What may be archived locally

- duplicate V1-V19 experiments outside protected release folders;
- abandoned autosaves and intermediate merge outputs after their final
  counterpart is verified;
- ad hoc test executables and temporary output folders that are not part of the
  pinned QLC+ installation or current package; and
- unrelated application artifacts that are outside the QLC+ integration
  QLC+ plug-in source or protected release evidence.

## What must remain

- the V26 current workspace and complete package;
- V24, V23, V22, V21, and V20 rollback packages;
- the V25 reviewed source workspace;
- the pinned coherent QLC+ installation and Qt/runtime files;
- plug-in backups and install receipt;
- `SoundSwitch-Control-One-Performance.qxi`;
- required fixture definitions and VirtualDJ OS2L/pad configuration; and
- the Git repository.

## Recovery-first cleanup

Prefer moving local legacy material into a dated archive over permanent
deletion. Resolve and review every target path before a recursive operation and
confirm it is inside the intended archive folder. Only consider permanent
removal after the archived system has survived normal use and the protected
rollback set is independently verified.
