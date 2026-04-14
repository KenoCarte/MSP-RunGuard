#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

echo "[1/5] Python syntax smoke..."
python3 -m py_compile analysis/risk_scoring.py
python3 -m py_compile analysis/run_temporal_analysis.py
python3 -m py_compile infer/infer_once.py

echo "[2/5] Required files..."
test -f analysis/risk_config.json
test -f infer/infer_once.py
test -f analysis/run_temporal_analysis.py

echo "[3/5] Qt configure..."
cmake -S qt_client -B qt_client/build

echo "[4/5] Qt build..."
cmake --build qt_client/build -j

echo "[5/5] Done. Smoke checks passed."
