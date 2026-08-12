# EmberLights 0.1.0-preview.84.1 Windows build evidence

This testing-preview package was built from GitHub commit
`ca1b3f72d65aa122daee7a65771346bb79bbd3dd`, whose complete source tree is
`05fba9358e6e30fc0a1830b5630c1ff7446a4ffd`.

## Local build boundary

- Cross-built for `x86_64-w64-windows-gnu` with llvm-mingw 20260616 / LLVM
  22.1.8. The publisher-supplied GitHub release digest for the toolchain
  archive was verified as
  `534b92e067b22a6b4441f48ae9240a3341b17825d04d577eab0cf85c44b4deda`.
- Configured with CMake 3.27.7 and compiled the production core,
  `EmberLights.exe`, and all five installed tools under the repository's
  warnings-as-errors policy. C++ and unwind runtimes are linked statically;
  PE imports contain Windows system libraries only.
- `EmberLights.exe` is a PE32+ x86-64 GUI application with the reviewed
  Windows manifest and version resource embedded. All installed tools are
  PE32+ x86-64 applications.
- The 17-file staged payload passed
  `installer/windows_package_contract.py create` and `verify`. Its manifest
  SHA-256 is
  `0abb365f564c149bcc8a849515741a2d2789ff08592d4d216edea2d8724908c0`.
- The NSIS compiler bundle was downloaded from the official
  electron-builder-binaries GitHub release and matched its published
  SHA-512. The installer was extracted on Linux; every embedded product file
  matched the staged payload byte-for-byte. A second independent installer
  build produced the same SHA-256.
- The portable ZIP was independently extracted and passed the payload
  contract again. A second archive build produced the same SHA-256.

## Important limits

This package is unsigned and has not yet been launched, installed, or
uninstalled on a native Windows machine. Windows may show a SmartScreen
warning. No physical fixture, USB output, reconnect, soak, or gig-readiness
claim is made. Outputs remain disabled by default and the package manifest
identifies this as a testing preview.

Use `SHA256SUMS.txt` to verify the downloaded files before installing.
