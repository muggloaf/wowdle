$ErrorActionPreference = "Stop"

if (-not (Get-Command em++ -ErrorAction SilentlyContinue)) {
    throw "em++ was not found. Install Emscripten and activate it before running this script."
}

New-Item -ItemType Directory -Force extension\data | Out-Null
Copy-Item data\wordlist_fives.txt extension\data\wordlist_fives.txt -Force
Copy-Item data\word_frequencies_fives.txt extension\data\word_frequencies_fives.txt -Force

em++ -std=c++17 -O3 -Isrc `
    src/wasm/solver_bridge.cpp `
    src/core/feedback.cpp `
    src/core/candidate_filter.cpp `
    src/probability/prior.cpp `
    src/strategies/entropy.cpp `
    -o extension\wowdle.js `
    -s MODULARIZE=1 `
    -s EXPORT_NAME=WowdleModuleFactory `
    -s ENVIRONMENT=web `
    -s ALLOW_MEMORY_GROWTH=1 `
    -s STACK_SIZE=1048576 `
    -s EXPORTED_FUNCTIONS='["_wowdle_init","_wowdle_recommend","_wowdle_submit"]' `
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString"]'

Write-Host "built extension/wowdle.js and extension/wowdle.wasm"
