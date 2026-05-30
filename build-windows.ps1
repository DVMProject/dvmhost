param(
    [string]$Generator = "Ninja",
    [string]$BuildDir = "out\\build\\windows",
    [string]$Config = "RelWithDebInfo"
)

function Abort($msg) {
    Write-Error $msg
    exit 1
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { Abort "cmake not found on PATH. Install CMake and retry." }
if ($Generator -eq "Ninja" -and -not (Get-Command ninja -ErrorAction SilentlyContinue)) { Abort "ninja not found on PATH. Install Ninja and retry." }

Write-Host "Configuring (Generator=$Generator, BuildDir=$BuildDir, Config=$Config)"
cmake -S . -B $BuildDir -G $Generator -DCOMPILE_WIN32=ON -DCMAKE_BUILD_TYPE=$Config
if ($LASTEXITCODE -ne 0) { Abort "CMake configure failed." }

Write-Host "Building"
cmake --build $BuildDir --config $Config -- -v
if ($LASTEXITCODE -ne 0) { Abort "Build failed." }

Write-Host "Build finished. Artifacts are in: $BuildDir"