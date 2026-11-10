param([switch]$Test)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Force .build | Out-Null
    $core = @('src/core/feedback.cpp', 'src/core/candidate_filter.cpp', 'src/probability/prior.cpp')
    $strategy = @('src/strategies/entropy.cpp', 'src/strategies/distance.cpp', 'src/strategies/risk.cpp', 'src/strategies/scoring.cpp', 'src/strategies/adaptive.cpp')
    $evaluation = @('src/evaluation/benchmark.cpp')
    & g++ -std=c++17 -O2 -Isrc src/main.cpp @core @strategy @evaluation -o .build/wowdle.exe
    if ($LASTEXITCODE -ne 0) { throw 'CLI build failed.' }
    if ($Test) {
        foreach ($suite in @('feedback', 'candidate_filter', 'prior', 'entropy', 'distance', 'risk', 'scoring', 'adaptive', 'benchmark', 'bridge')) {
            $extra = @()
            if ($suite -eq 'bridge') { $extra = @('src/wasm/solver_bridge.cpp') }
            $output = ".build/test_$suite.exe"
            & g++ -std=c++17 -O2 -Isrc @core @strategy @evaluation @extra "tests/test_$suite.cpp" -o $output
            if ($LASTEXITCODE -ne 0) { throw "Could not build $suite tests." }
            & ".\$output"
            if ($LASTEXITCODE -ne 0) { throw "$suite tests failed." }
        }
        & node --test tests/test_content.js tests/test_extension.js tests/test_wasm.js
        if ($LASTEXITCODE -ne 0) { throw 'Extension tests failed.' }
    }
    Write-Host 'built .build/wowdle.exe'
} finally {
    Pop-Location
}
