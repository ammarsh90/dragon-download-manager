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

### 1.2.1 interface update

The Windows executable compiled and linked successfully. Light and dark progress windows, expanded details, the compact add form and main window were rendered for visual review. No new behavioral tests were run. Tray behavior is implemented with an availability guard and a separate explicit Exit action.

### 1.3 browser integration

Python 3 packages the browser assets as part of the default CMake build (BUILD_LAUNCHER=ON). It builds DragonBrowserHost and both extension variants. Chromium's committed public manifest key fixes the unpacked extension ID and must stay in sync with the native host's allowed_origins. No private signing key is included. Firefox output is an unsigned development ZIP, not a signed release add-on.

The old launcher and old locale names are removed from staged packages. Registration is prepared in NSIS (HKLM) and register-browser.ps1 (HKCU); building the installer does not install or register anything on the developer machine.

The 1.3 Windows app and DragonBrowserHost compiled and linked successfully. JavaScript syntax, JSON parsing, native-host origin identity and PowerShell script parsing passed static checks. The desktop browser-setup and received-file dialogs were rendered for visual review. No behavioral browser or network tests were run.
