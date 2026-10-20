# Building Dragon Download Manager

## Windows

C++17, Qt 6.5.3, CMake, Ninja, MinGW, Boost, OpenSSL 3, Gumbo and libtorrent are used. The checked-in `3rd` tree contains upstream source dependencies. Local downloaded toolchains live in ignored `.deps` and `.build-tools` directories.

The reproducible local build script is:

```powershell
./tools/build_windows.ps1 -Jobs 4
```

Toolchain paths can be overridden with `-QtRoot`, `-CompilerRoot`, `-BoostRoot`, and `-OpenSSLRoot`. It builds libtorrent, configures the Qt application, and stages runtime DLLs under `dist/DragonDownloadManager`. Use matching MinGW runtime DLLs from the compiler actually used to build the application.

For an existing configured tree:

```powershell
cmake --build build/application --parallel 4
cmake --install build/application
```

NSIS setup source is `installer/windows/NSIS/setup.nsi`. Package only the staged runtime folder, preserve `LICENSE.txt`, and include SHA-256 checksums when publishing. Credentials, build outputs and downloaded toolchains must not be committed.

## Verification status

The 1.2.0 Windows application compiled and linked successfully with the local toolchain. Main, add-download and progress screens were rendered for visual review. This is a preview release.

The 1.1.1 release had focused download and resume checks. Those checks do not establish coverage for the new segmented 1.2 engine. No new behavioral tests were run for this change unless explicitly recorded in the release notes. The inherited full CI previously had four timed-out tests; do not interpret its overall status as a complete behavioral pass.
