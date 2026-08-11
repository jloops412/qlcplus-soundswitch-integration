# Linux installation

Linux packages are testing previews of the portable EmberLights core. The
native Studio/Live desktop application remains Windows-first; the Linux package
installs the deterministic Runner lab, MIDI discovery, qualification, and
migration command-line tools.

## Debian or Ubuntu

Install the package from the release artifact directory:

```bash
sudo apt install ./EmberLights-<version>-Linux-<architecture>.deb
```

Run the installed smoke checks:

```bash
runner_lab --os2l-port 0 --duration 1 --manual-bpm 120
emberlights_qualify --duration 2 --network-loopback --report qualification.json
emberlights_migrate --help
midi_capture --list
```

Remove it with:

```bash
sudo apt remove emberlights
```

## Portable archive

The matching `.tar.gz` contains the same `/usr`-relative layout and is intended
for inspection or systems where installing a DEB is inappropriate. Extract it
into a temporary directory and run tools from `usr/bin`.

Verify either artifact against the accompanying SHA-256 file before use. These
packages do not claim Linux desktop parity, SoundSwitch Micro access, or
gig-qualified hardware output; those remain Windows/physical qualification
gates.
