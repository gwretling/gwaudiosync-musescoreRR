$ErrorActionPreference = 'Stop'
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
Write-Host "Done. Look for *.vst3 in build/ or the common VST3 folders."
