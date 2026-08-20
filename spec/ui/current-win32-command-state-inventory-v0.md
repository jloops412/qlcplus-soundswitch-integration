# Current Win32 Command and State Inventory v0

Status: migration inventory for issue #31. This is not the final generated registry; it is the authoritative starting map from `native-core/src/windows_app.cpp` into product-semantic commands and observable state.

## Source baseline

The current shell contains:

- `Page`: Live, Overrides, Profiles, Patch, Groups, Looks, Autoloops, Tracks, MIDI, Connections, Safety, Diagnostics;
- menu and accelerator commands;
- `ControlId` values grouped by page;
- direct `WM_COMMAND` dispatch;
- page-specific refresh functions;
- direct Runner and Project mutations;
- F5 show start/stop and F8 blackout;
- a 250 ms status refresh;
- app-local last-project path persistence.

The facade migration keeps the current controls operational while replacing direct control-ID-to-domain calls.

## Command classes

| Class | Meaning | Delivery expectation |
| --- | --- | --- |
| `emergency` | Blackout and fail-safe stop paths | Non-droppable; must not depend on ordinary bounded command queue success |
| `priority-live` | Work light, hazard disarm, Release All, output stop/reconnect | Bounded priority path with explicit result |
| `live` | Autoloops, Looks, Track Scripts, BPM, overrides, bank navigation | Bounded Runner command |
| `studio` | Project/fixture/group/look/loop/script/mapping mutations | UI/Studio controller; Undo semantics declared |
| `blocking-utility` | File dialogs, imports, reports, comparison/bundling | Never on Runner/scheduler thread |
| `view-local` | Workspace/panel/selection/navigation state | App-local; never changes lighting by itself |

## Invocation result contract

Every command invocation returns one of:

```text
accepted
acceptedPending
noChange
unavailable
invalidArguments
notFound
safetyRejected
validationFailed
queueFull
busy
requiresReconnect
requiresRestart
ioFailed
cancelled
unsupported
internalError
```

The UI, MIDI feedback, tests, and external surfaces must not infer success merely because an input event was received.

## Global menu and accelerator mapping

| Legacy ID | Proposed command | Class | Persistence | Feedback/state |
| --- | --- | --- | --- | --- |
| `IdFileNew` | `project.new` | studio | project | `project.lifecycle`, `project.saveState` |
| `IdFileOpen` | `project.open.dialog` | blocking-utility | app/project | `project.lifecycle`, `project.path` |
| `IdFileSave` | `project.save` | studio | project | `project.saveState`, `project.lastVerifiedSave` |
| `IdFileSaveAs` | `project.saveAs.dialog` | blocking-utility | project/app | save/path states |
| `IdFileRestoreHistory` | `project.restoreHistory.open` | blocking-utility | project | `project.history.*` |
| `IdFileImportSoundSwitch` | `migration.soundswitch.import.dialog` | blocking-utility | project | `migration.lastReport.*`, project lifecycle/result |
| `IdFileInspectSoundSwitch` | `migration.soundswitch.inspect.dialog` | blocking-utility | none | `migration.lastReport.*` |
| `IdFileCompareSoundSwitch` | `migration.soundswitch.compare.dialog` | blocking-utility | none | report/progress/error |
| `IdFileBundleSoundSwitch` | `migration.soundswitch.bundle.dialog` | blocking-utility | none | report/progress/error |
| `IdFileExit` | `app.quit.request` | view-local | app | dirty/save prompt state |
| `IdEditUndo` | `undo.perform` | studio | project | `undo.canUndo`, `undo.label`, save state |
| `IdEditRedo` | `redo.perform` | studio | project | `undo.canRedo`, `redo.label`, save state |
| `IdShowValidate` | `project.validate` | studio | none | validation summary |
| `IdShowStartStop` / F5 | `show.toggleRunning` | priority-live | live-transient | `runner.state`, activation error |
| `IdHelpAbout` | `app.about.open` | view-local | none | none |
| F8 | `output.blackout.toggle` | emergency | live-transient | `safety.blackout` |

### Start/stop split

The public registry should also expose explicit commands:

```text
show.start
show.stop
show.toggleRunning
```

Automation and safety-sensitive surfaces should prefer explicit start or stop over a toggle.

## Navigation mapping

Legacy navigation IDs become view commands, not domain modes:

| Legacy page | Proposed command/panel target |
| --- | --- |
| Live | `workspace.open("live")` |
| Overrides | `workspace.open("live"); view.livePage.open("overrides")` |
| Profiles | `workspace.open("studio"); view.panel.open("profiles")` |
| Patch | `workspace.open("studio"); view.panel.open("patch")` |
| Groups | `workspace.open("studio"); view.panel.open("groups")` |
| Looks | `workspace.open("studio"); view.panel.open("staticLooks")` |
| Autoloops | `workspace.open("studio"); view.panel.open("autoloops")` |
| Tracks | `workspace.open("studio"); view.panel.open("trackScripts")` |
| MIDI | `view.panel.open("mapping")` |
| Connections | `view.drawer.open("connections")` |
| Safety | `view.drawer.open("safety")` |
| Diagnostics | `view.drawer.open("diagnostics")` |

View selection is app-local and may be remembered, but it never changes Runner behavior by itself.

## Live command mapping

| Legacy control | Proposed command | Class | Arguments | Feedback/state |
| --- | --- | --- | --- | --- |
| `IdLiveStartStop` | `show.toggleRunning` | priority-live | none | `runner.state` |
| `IdLiveBlackout` | `output.blackout.toggle` | emergency | none | `safety.blackout` |
| `IdLiveWorkLight` | `output.workLight.toggle` | priority-live | none | `safety.workLight` |
| `IdLiveApplyBpm` | `transport.manualBpm.set` | live | `bpm` | `transport.manualBpm`, sync state |
| `IdLiveTap` | `transport.tap` | live | optional timestamp | `transport.bpm`, phase/confidence |
| `IdLiveTriggerLook` | `staticLook.activate` | live | stable look ID | active look state |
| `IdLiveClearLook` | `staticLook.clear` | live | fade override optional | active look state |
| `IdLiveTriggerAutoloop` | `autoloop.launch` | live | bank/slot or stable ID | active/progress state |
| `IdLivePreviousAutoloop` | `autoloop.previous` | live | none | active ID/address |
| `IdLiveNextAutoloop` | `autoloop.next` | live | none | active ID/address |
| `IdLiveClearAutoloop` | `autoloop.clear` | live | release policy optional | active/progress state |
| `IdLiveTriggerTrack` | `trackScript.start` | live | stable script ID | active/elapsed/cue count |
| `IdLiveClearTrack` | `trackScript.clear` | live | none | active script state |
| `IdLiveFogArm` | `safety.hazard.setArmed` | priority-live | type=`fog`, armed | `safety.hazard.fog.armed` |
| `IdLiveHazeArm` | `safety.hazard.setArmed` | priority-live | type=`haze`, armed | haze armed |
| `IdLiveLaserArm` | `safety.hazard.setArmed` | priority-live | type=`laser`, armed | laser armed |
| `IdLiveSparkArm` | `safety.hazard.setArmed` | priority-live | type=`spark`, armed | spark armed |
| previous bank page | `autoloop.bankWindow.previous` | view-local/live | none | visible bank window |
| next bank page | `autoloop.bankWindow.next` | view-local/live | none | visible bank window |
| select all banks | `autoloop.bankFilter.enableAll` | live | none | enabled-bank mask |
| bank enable controls | `autoloop.bankFilter.setEnabled` | live | bank, enabled | enabled-bank mask |
| bank-only controls | `autoloop.bankFilter.selectExclusive` | live | bank | enabled-bank mask |

### Live state-only controls

The following do not require commands:

```text
IdLiveTitle
IdLiveState
IdLiveMetrics
IdLiveAutoloopBankPage
IdLiveAutoloopPlayback
IdLiveTrackLabel
```

They bind to shared state.

### Required Live state keys

```text
runner.state
runner.generation
runner.health
runner.lastError
runner.jitter.p99Ms
runner.jitter.maxMs
runner.frameRate
project.active.id
project.active.name
project.active.generation
transport.source.kind
transport.source.status
transport.syncState
transport.bpm
transport.phase
transport.confidence
transport.lastEventAgeMs
output.blackout
output.workLight
safety.hazard.fog.armed
safety.hazard.haze.armed
safety.hazard.laser.armed
safety.hazard.spark.armed
autoloop.active.id
autoloop.active.bank
autoloop.active.slot
autoloop.active.progress
autoloop.active.repeat
autoloop.active.completedCycles
autoloop.bankWindow.firstBank
autoloop.bankWindow.visibleBanks[]
autoloop.bankFilter.mask
staticLook.active.id
trackScript.active.id
trackScript.elapsedBeat
trackScript.consumedCueCount
override.activePropertyCount
```

## Override mapping

| Legacy control | Proposed command/state |
| --- | --- |
| fixture/group selector | state/selection: `view.overrideTarget.*` |
| property selector | state/selection: `view.overrideProperty` |
| value field | state/selection: `view.overrideValue` |
| Apply | `override.property.set(target, property, normalizedValue)` |
| Release | `override.property.release(target, property)` |
| Release All | `override.releaseAll` — `priority-live` |
| active count | `override.activePropertyCount` |
| help/message | command metadata and invocation result, not bespoke text logic |

Target resolution must occur through stable fixture/group IDs. A group override remains one validated Runner command with an immutable member mask; the UI must not loop over fixtures.

## Fixture-profile authoring mapping

| Legacy control | Proposed command/state |
| --- | --- |
| list/select | `view.selection.profile.set(profileId)` |
| Import QLC | `profile.importQxf.dialog` |
| New | `profile.create` |
| Duplicate | `profile.duplicate(profileId)` |
| Save | transitional `profile.editor.commit`; target design uses field-level `profile.update` |
| Delete | `profile.delete.request(profileId)` |
| manufacturer/model/mode/name/footprint/channels | editor draft state, validated into `profile.update` |
| Named DMX ranges | transitional `profile.channelCapability.open/upsert/remove`; target uses typed draft state plus `profile.update` |
| capability name/property/from/to/preferred/behavior/access/role/reverse | structured editor draft; stable ID is generated, never typed |
| channel owner/blackout/highlight | structured editor draft, validated into `profile.update` |
| help | Command Explorer/context help |
| message | validation/invocation result state |

Persistence: project-authored. Undoable: yes, except opening dialogs. Import records one coherent Undo transaction after successful validation.

## Patch/fixture mapping

| Legacy control | Proposed command/state |
| --- | --- |
| fixture list/select | `view.selection.fixture.set(fixtureId)` |
| New | `fixture.create` |
| Save | transitional `fixture.editor.commit`; target `fixture.update`/`fixture.repatch` |
| Delete | `fixture.delete.request` |
| name/profile/universe/address/roles | draft fields compiled into typed update |
| overlap/range messages | `patch.validation.*` state |

Persistence: project-authored. Undoable: yes. Address changes must validate complete patch state before commit.

## Group mapping

| Legacy control | Proposed command/state |
| --- | --- |
| list/select | `view.selection.group.set(groupId)` |
| New | `group.create` |
| Duplicate | `group.duplicate(groupId)` |
| Save | `group.update(groupId, fields)` |
| Delete | `group.delete.request(groupId)` |
| name/members | editor draft and validation state |

The final group editor should support search/filter and stable fixture membership rather than comma-separated text as the primary UX.

## Static Look mapping

| Legacy control | Proposed command/state |
| --- | --- |
| list/select | `view.selection.staticLook.set(lookId)` |
| New | `staticLook.create` |
| Duplicate | `staticLook.duplicate` |
| Save | `staticLook.update` |
| Delete | `staticLook.delete.request` |
| name/fade | typed draft fields |
| assignments | `staticLook.assignment.setOwnerState(target, property, RELEASE|SET|FORCE_ZERO)` and value commands |

The modern editor must expose ownership semantics directly. Excluded/released is not the same as included with a zero value.

## Autoloop authoring mapping

| Legacy control | Proposed command/state |
| --- | --- |
| list/select | `view.selection.autoloop.set(loopId)` |
| New | `autoloop.create` |
| Duplicate | `autoloop.duplicate` |
| Save | `autoloop.update` |
| Delete | `autoloop.delete.request` |
| Next Empty | `autoloop.moveToNextEmpty` |
| Swap Target | `autoloop.swap` |
| name | draft field |
| bank/slot | stable-address placement command |
| length | typed musical length |
| repeat | Once/Infinite/TrackDuration |
| steps | structured native editor; text bridge may remain temporarily |

Required later commands:

```text
autoloop.move
autoloop.swap
autoloop.populateEmpty
autoloop.resetDefaults.request
autoloop.reorder
autoloop.copyToSlot
```

Every destructive population/reset action requires preview/confirmation and project history.

## Track Script mapping

| Legacy control | Proposed command/state |
| --- | --- |
| list/select | `view.selection.trackScript.set(scriptId)` |
| New | `trackScript.create` |
| Duplicate | `trackScript.duplicate` |
| Save | `trackScript.update` |
| Delete | `trackScript.delete.request` |
| audio asset select | `trackScript.audio.bind` |
| Add Audio | `audioAsset.add.dialog` |
| Relink | `audioAsset.relink.dialog` |
| Verify | `audioAsset.verify` |
| Resolve Folder | `audioAsset.resolveFolder.dialog` |
| audio key/cues | draft state; later native association/timeline components |

File hashing/scanning is blocking utility work and must publish progress/cancellation state rather than freeze the UI.

## MIDI mapping

| Legacy control | Proposed command/state |
| --- | --- |
| list/select | `view.selection.mapping.set(mappingId)` |
| action | command-registry selection |
| target/property/behavior | typed binding editor fields |
| soft takeover | binding transform option |
| Learn | `binding.learnMidi.start` |
| Delete | `binding.delete.request` |
| learn result/message | `binding.learn.*` state and invocation result |

The mapping editor must query command metadata rather than maintain a separate hard-coded action enum in the UI.

## Connections and output mapping

The current `IdConnectionsApply` remains a temporary bridge. The target design is field-level persistence with explicit activation outcome.

| Legacy field/control | Proposed command | Scope | Activation |
| --- | --- | --- | --- |
| project name | `project.name.set` | project | immediate document mutation |
| OS2L enabled | `connection.os2l.setEnabled` | project | connect/disconnect immediately |
| OS2L bind | `connection.os2l.setBindAddress` | project + machine validation | reconnect required |
| OS2L port | `connection.os2l.setPort` | project | reconnect required |
| Art-Net enabled | `output.artnet.setEnabled` | project | immediate/reconnect |
| Art-Net destination | `output.artnet.setDestination` | project | immediate adapter reconfigure |
| Art-Net base universe | `output.artnet.setBaseUniverse` | project | reconfigure |
| sACN enabled | `output.sacn.setEnabled` | project | immediate/reconnect |
| sACN destination | `output.sacn.setDestination` | project | reconfigure |
| sACN base universe | `output.sacn.setBaseUniverse` | project | reconfigure |
| DMX USB Pro U1/U2 device | `output.dmxUsbPro.setDevice` | project logical + app-local hardware hint | reconnect |
| SoundSwitch Micro universe | `output.soundSwitchMicro.setUniverse` | project | reconnect |
| SoundSwitch Micro framing | `output.soundSwitchMicro.setFraming` | project | reconnect; diagnostic only while framing is not user-facing |
| frame rate | `output.frameRate.set` | project | controlled reconfigure |
| manual BPM | `transport.manualBpm.setDefault` | project | immediate |
| MIDI input/output | `controller.portPreference.set` | app-local/profile | reconnect |
| Refresh MIDI | `controller.devices.refresh` | blocking/priority | no persistence |
| Apply bridge | `connections.editor.commit` | transitional | must return per-field activation results |

### Connection state keys

```text
connection.os2l.configured
connection.os2l.status
connection.os2l.endpoint
connection.os2l.clientCount
connection.os2l.lastError
connection.os2l.lastEventAgeMs
controller.input.status
controller.output.status
controller.deviceCount
controller.lastError
output.universe[0].adapterKind
output.universe[0].status
output.universe[0].lastError
output.universe[0].framesAccepted
output.universe[0].framesFailed
output.universe[1].*
output.soundSwitchMicro.status
output.soundSwitchMicro.lastWindowsError
output.soundSwitchMicro.nonzeroSlotCount
settings.pendingReconnect[]
settings.pendingRestart[]
```

## Safety mapping

Hazard arming and authored limits are separate command classes and persistence scopes.

| Legacy control | Proposed command | Scope |
| --- | --- | --- |
| Fog/Haze/Laser/Spark arm | `safety.hazard.setArmed` | live-transient, fail-closed |
| strobe allowed | `safety.policy.strobeAllowed.set` | project-authored |
| max strobe | `safety.policy.maxStrobe.set` | project-authored |
| max intensity | `safety.policy.maxIntensity.set` | project-authored |
| Apply | transitional `safety.policy.editor.commit` | project-authored |

Arming state must not be restored automatically across application restart, project activation, or unsafe connection loss unless a future approved hardware interlock design explicitly permits it.

## Diagnostics mapping

| Legacy control | Proposed command/state |
| --- | --- |
| text view | native Diagnostics component consuming structured state/log records |
| Copy | `diagnostics.copySummary` |
| Export | `diagnostics.exportReport.dialog` |
| Validate | `project.validate` or `qualification.runSelected` depending context |

The long-term UI should not parse formatted diagnostics text to derive status. Structured state is primary; text export is a presentation/report format.

## Project/document state keys

```text
project.id
project.name
project.path
project.lifecycle
project.saveState
project.dirty
project.recoveryRequired
project.lastVerifiedSave
project.lastError
project.activeVenueId
project.validation.errorCount
project.validation.warningCount
project.validation.summary
undo.canUndo
undo.canRedo
undo.undoLabel
undo.redoLabel
selection.profileId
selection.fixtureId
selection.groupId
selection.staticLookId
selection.autoloopId
selection.trackScriptId
selection.mappingId
```

## Persistence scopes

### App-local

- last project path and reopen preference;
- selected skin and responsive variant override;
- window/monitor geometry;
- current workspace/panels;
- recent paths;
- local audio path hints;
- local MIDI/USB device match hints;
- reduced-motion/accessibility preferences.

### Project-authored

- project/venue/fixture/group/profile data;
- Looks, Autoloops, scripts, mappings;
- logical adapter configuration and universe routing;
- safety policy limits;
- default manual BPM and show preferences.

### Live-transient

- active Look/Autoloop/Track Script;
- bank filter/window where not explicitly saved as user layout;
- manual overrides;
- blackout/work light;
- hazard arming;
- temporary master/group/intensity values;
- reconnect/backoff state.

## Facade extraction order

### Slice 1 — Safety and Live core

- F8/blackout;
- start/stop;
- work light;
- hazard arming;
- Release All;
- manual BPM/tap.

Purpose: prove command/result/state flow without changing ordinary authoring.

### Slice 2 — Active content

- Static Looks;
- Autoloops and bank state;
- Track Scripts;
- active/progress feedback.

Purpose: prove shared behavior across legacy shell, keyboard, MIDI, and future skins.

### Slice 3 — Connections and health

- OS2L/output/controller commands;
- auto-reconnect actions;
- actionable state;
- field-level persistence outcomes.

### Slice 4 — Project lifecycle

- new/open/save/save-as/history/validate;
- Undo/Redo;
- migration/report utilities.

### Slice 5 — Authoring CRUD

- profiles, patch, groups, Looks, Autoloops, scripts, mappings, safety policy.

### Slice 6 — View/navigation cleanup

- legacy page navigation becomes one view adapter;
- direct callback bypass list reaches zero or documented exceptions.

## Direct-call exception policy

A legacy direct call may remain temporarily only when:

1. it is documented in the bypass ledger;
2. it has no duplicate behavior in another surface;
3. it cannot affect Runner/live output without the approved safety boundary;
4. a specific follow-up issue and owner exist;
5. the legacy shell is the only caller.

No new UI feature may add a new direct callback bypass after the facade milestone begins.

## Cross-surface conformance suite

The following representative actions must produce the same domain result and feedback regardless of invocation source:

```text
output.blackout.set(true/false)
output.workLight.toggle
override.releaseAll
transport.manualBpm.set
autoloop.launch
autoloop.clear
autoloop.bankFilter.selectExclusive
staticLook.activate
staticLook.clear
trackScript.start
trackScript.clear
group.override.property.set
connection.os2l.reconnect
project.save
```

Sources tested:

- legacy Win32 adapter while it exists;
- EmberLights Default;
- SoundSwitch Reference;
- keyboard;
- MIDI test mapping;
- direct command test invocation.

## Open decisions for implementation evidence

- concrete generated-registry representation in C++;
- exact state subscription transport between Runner and UI thread;
- whether view-local state shares the product registry or a parallel namespaced app-state service;
- transaction shape for field-level authoring edits versus editor-draft commit;
- exact priority queue integration without weakening current non-droppable behavior;
- stable public localization-key naming.

These decisions must not change the stable semantic command IDs without updating this inventory and the versioned registry contract.
