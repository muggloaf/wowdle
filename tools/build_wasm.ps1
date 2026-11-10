$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$compiler = Get-Command em++ -ErrorAction SilentlyContinue
$compilerPath = if ($compiler) { $compiler.Source } else { $null }
if (-not $compiler) {
    $localCompiler = Join-Path $projectRoot '..\emsdk\upstream\emscripten\em++.exe'
    if (Test-Path -LiteralPath $localCompiler) { $compilerPath = $localCompiler }
}
if (-not $compilerPath) {
    throw "em++ was not found. Install Emscripten and activate it before running this script."
}

Push-Location $projectRoot
try {
New-Item -ItemType Directory -Force extension\data | Out-Null
Copy-Item data\wordlist_fives.txt extension\data\wordlist_fives.txt -Force
Copy-Item data\word_frequencies_fives.txt extension\data\word_frequencies_fives.txt -Force

& $compilerPath -std=c++17 -O3 -fexceptions -Isrc `
    src/wasm/solver_bridge.cpp `
    src/core/feedback.cpp `
    src/core/candidate_filter.cpp `
    src/probability/prior.cpp `
    src/strategies/scoring.cpp `
    src/strategies/risk.cpp `
    src/strategies/adaptive.cpp `
    -o extension\wowdle.js `
    -s MODULARIZE=1 `
    -s EXPORT_NAME=WowdleModuleFactory `
    -s ENVIRONMENT=web `
    -s ALLOW_MEMORY_GROWTH=1 `
    -s STACK_SIZE=1048576 `
    -s EXPORTED_FUNCTIONS='["_wowdle_init","_wowdle_recommend","_wowdle_submit","_wowdle_restore"]' `
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString"]'

if ($LASTEXITCODE -ne 0) { throw "WebAssembly build failed (exit $LASTEXITCODE)." }
# recreate the generated loader so it inherits the project folder's permissions.
$loaderPath = Join-Path $projectRoot 'extension\wowdle.js'
$loaderBytes = [System.IO.File]::ReadAllBytes($loaderPath)
Remove-Item -LiteralPath $loaderPath
[System.IO.File]::WriteAllBytes($loaderPath, $loaderBytes)
Write-Host "built extension/wowdle.js and extension/wowdle.wasm"
} finally {
    Pop-Location
}
