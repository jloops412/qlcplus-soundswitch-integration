# Post-Test Promotion and Local Cleanup

Do not run local cleanup until the V23 focused owner test passes. Cleanup should make the current release obvious without destroying rollback evidence.

## Promotion rule

After V23 passes:

1. Close QLC+ so no workspace or plug-in file is in use.
2. Keep one clearly named working copy: `IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw`.
3. Keep the downloaded V23 ZIP and SHA-256 file unchanged as the release source of truth.
4. Keep V22 as the unified creative rollback, V21 as the reliability rollback, and V20 as the protected creative rollback.
5. Keep the installer-created plug-in backup and receipt with the pinned QLC+ installation.
6. Move earlier experiments and duplicate numbered workspaces into one dated `Legacy-Pre-V23` archive folder.
7. Do not delete fixture definitions, VirtualDJ mappings, Control One profiles, QLC+ core/runtime files, or files whose purpose has not been verified.

## What may be archived

- duplicate V1–V19/V20/V21 experiments outside the protected release folders;
- abandoned autosaves and intermediate merge outputs after their final counterpart is verified;
- old ad hoc test executables and temporary output folders that are not part of the pinned QLC+ installation or V22 release; and
- retired standalone EmberLights application artifacts, provided no QLC+ plug-in source, research evidence, or release file is mixed into the target.

## What must remain

- V23 current workspace and release archive;
- V22 unified creative rollback workspace/package;
- V21 reliability rollback workspace/package;
- V20 creative rollback workspace/package;
- current pinned QLC+ installation and coherent Qt/runtime files;
- `soundswitch.dll` backup/receipt;
- `SoundSwitch-Control-One-Performance.qxi`;
- required fixture definitions and VirtualDJ OS2L/pad configuration; and
- the Git repository.

## Recovery-first cleanup

Prefer a move into the dated archive over permanent deletion. Before moving anything recursively, resolve and review every absolute target path and confirm it is inside the intended legacy folder. Once the archived system has survived normal use, the owner can decide whether old material should be compressed or removed.
