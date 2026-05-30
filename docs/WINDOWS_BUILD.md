Windows build prerequisites and steps for dvmhost

Prerequisites
- Visual Studio 2019/2022 (Desktop development with C++ workload) or at least the MSVC build tools.
- CMake 3.16 or newer
- Ninja (recommended generator)
- Git
- (Optional) OpenSSL for Windows if you need SSL; not required by default when building with `-DCOMPILE_WIN32=ON`.

Quick steps
1. Open a Developer command prompt (e.g. "x64 Native Tools Command Prompt for VS 2022") or run the MSVC environment so compilers are on PATH.
2. Ensure `cmake` and `ninja` are on PATH.
3. From the repository root:

```powershell
# create an out/build directory and configure
mkdir -p out\build\windows
cmake -S . -B out\build\windows -G Ninja -DCOMPILE_WIN32=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo

# build
cmake --build out\build\windows --config RelWithDebInfo
```

Notes
- The project provides `CMakeSettings.json` presets for Visual Studio Code/CMake Tools which enable `COMPILE_WIN32` and use Ninja.
- TUI support is disabled on Windows by design; some utilities that require ncurses will not be available when `COMPILE_WIN32=ON`.
- If you prefer Visual Studio IDE: open the CMake project in Visual Studio, select the provided configuration (see `CMakeSettings.json`) and build.
- If you need OpenSSL for Windows, install a binary distribution and set `-DOPENSSL_ROOT_DIR="C:/path/to/openssl"` when invoking `cmake`.

If you want, run `.uild-windows.ps1` from a Developer PowerShell prompt — it will run the configure+build steps automatically.