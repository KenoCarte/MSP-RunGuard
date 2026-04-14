$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host "[1/5] Python syntax smoke..."
python -m py_compile analysis/risk_scoring.py
python -m py_compile analysis/run_temporal_analysis.py
python -m py_compile infer/infer_once.py

Write-Host "[2/5] Required files..."
if (!(Test-Path analysis/risk_config.json)) { throw "Missing analysis/risk_config.json" }
if (!(Test-Path infer/infer_once.py)) { throw "Missing infer/infer_once.py" }
if (!(Test-Path analysis/run_temporal_analysis.py)) { throw "Missing analysis/run_temporal_analysis.py" }

Write-Host "[3/5] Qt configure..."
cmake -S qt_client -B qt_client/build

Write-Host "[4/5] Qt build..."
cmake --build qt_client/build -j

Write-Host "[5/5] Done. Smoke checks passed."