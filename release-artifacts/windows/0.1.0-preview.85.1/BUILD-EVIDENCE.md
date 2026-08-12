# EmberLights 0.1.0-preview.85.1 Windows build evidence

This testing-preview package was built from GitHub commit
`8c6f803dc92ec98c182d9666e01c591b90fe98fa`, whose complete source tree is
`a259d69d4a26c9cf9f662134e0ceb2c98f4c8a5f`. The source is reviewed in
[pull request #85](https://github.com/jloops412/EmberLights/pull/85).

## Source and regression evidence

- The packaged source commit is exactly two commits ahead of production-integration commit
  `ca1b3f72d65aa122daee7a65771346bb79bbd3dd`.
- The Linux CMake build completed and all 28 CTest targets passed.
- All 15 Windows package-contract unit tests passed. The subsequent PR commit
  `f8337c69d5d9603460bd27107450470d3bd0cccb` makes the runtime-closure
  audit fail closed for future packages; it does not alter the packaged app
  binaries.
- The generated UI registry remained at generation 2 with 29 commands,
  39 states, and digest
  `d3f5c6edc1226a5184ddcf7d7ed2405605534131e6c6ab88b167f111b1614945`.

## Windows cross-build boundary

- Cross-built for `x86_64-w64-windows-gnu` with llvm-mingw 20260616 /
  LLVM 22.1.8. The publisher-supplied GitHub release digest for the
  toolchain archive was verified as
  `534b92e067b22a6b4441f48ae9240a3341b17825d04d577eab0cf85c44b4deda`.
- Configured with CMake 3.27.7, `-static`, and the required llvm-rc
  temporary-file mode. The production core, Windows GUI, installed tools,
  Windows test executables, and benchmarks all compiled under the
  repository's warnings-as-errors policy.
- `EmberLights.exe` is a PE32+ x86-64 GUI application with the reviewed
  Windows manifest and version resource embedded. All five installed tools
  are PE32+ x86-64 applications. Their imports contain Windows system/API-set
  libraries only; libc++ and unwind runtimes are linked statically.
- The 18-file staged payload passed
  `installer/windows_package_contract.py create` and `verify`. Its
  manifest SHA-256 is
  `29b113fa7460fc36000d2a846af679ecdfc9328cf7dea4e4ac7856da96edbf5e`.
- The NSIS compiler bundle came from the official electron-builder-binaries
  GitHub release and matched its published SHA-512. The installer was
  extracted on Linux; all 18 embedded product files and the embedded
  manifest matched the staged payload byte-for-byte. A second independent
  installer build produced the same SHA-256.
- The portable ZIP was independently extracted and passed the payload
  contract. A second archive build produced the same SHA-256.

## Important limits

This package is unsigned and has not yet been launched, installed, or
uninstalled on a native Windows machine. Windows may show a SmartScreen
warning. No physical fixture, USB output, reconnect, soak, or gig-readiness
claim is made. Outputs remain disabled by default and the package manifest
identifies this as a testing preview.

Use `SHA256SUMS.txt` to verify the downloaded files before installing.
