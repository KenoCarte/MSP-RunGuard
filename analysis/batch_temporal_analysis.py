#!/usr/bin/env python3
"""Batch runner for stage-A/B analysis.

Given a directory of videos, run temporal analysis for each video and
export an aggregated summary CSV for thesis tables.
"""

from __future__ import annotations

import argparse
import csv
import json
import subprocess
import sys
from pathlib import Path
from typing import List


PROJECT_ROOT = Path(__file__).resolve().parents[1]
ANALYZER = PROJECT_ROOT / "analysis" / "run_temporal_analysis.py"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Batch temporal analysis")
    parser.add_argument("--video-dir", required=True, type=str)
    parser.add_argument("--out-root", required=True, type=str)
    parser.add_argument("--cfg", default="./experiments/vgg19_368x368_sgd.yaml", type=str)
    parser.add_argument("--weight", default="./network/weight/best_pose.pth", type=str)
    parser.add_argument("--device", default="auto", choices=["auto", "cuda", "cpu"])
    parser.add_argument("--frame-stride", default=1, type=int)
    parser.add_argument("--risk-config", default="", type=str)
    parser.add_argument("--save-overlay-video", action="store_true")
    return parser.parse_args()


def list_videos(video_dir: Path) -> List[Path]:
    exts = {".mp4", ".avi", ".mov", ".mkv", ".webm"}
    return sorted([p for p in video_dir.rglob("*") if p.is_file() and p.suffix.lower() in exts])


def main() -> int:
    args = parse_args()
    video_dir = Path(args.video_dir).resolve()
    out_root = Path(args.out_root).resolve()
    out_root.mkdir(parents=True, exist_ok=True)

    if not video_dir.exists():
        print(f"[ERROR] video dir not found: {video_dir}", file=sys.stderr)
        return 2

    videos = list_videos(video_dir)
    if not videos:
        print(f"[ERROR] no video found in {video_dir}", file=sys.stderr)
        return 3

    rows = []
    failed = 0
    for i, vp in enumerate(videos, start=1):
        case_dir = out_root / vp.stem
        cmd = [
            sys.executable,
            str(ANALYZER),
            "--cfg", args.cfg,
            "--weight", args.weight,
            "--video", str(vp),
            "--out-dir", str(case_dir),
            "--device", args.device,
            "--frame-stride", str(args.frame_stride),
        ]
        if args.risk_config:
            cmd.extend(["--risk-config", args.risk_config])
        if args.save_overlay_video:
            cmd.append("--save-overlay-video")

        print(f"[INFO] ({i}/{len(videos)}) run: {vp.name}")
        proc = subprocess.run(cmd, cwd=str(PROJECT_ROOT), text=True, capture_output=True)
        if proc.returncode != 0:
            failed += 1
            print(f"[FAIL] {vp.name} rc={proc.returncode}")
            print(proc.stderr.strip())
            continue

        summary_path = case_dir / "summary.json"
        if not summary_path.exists():
            failed += 1
            print(f"[FAIL] summary missing: {summary_path}")
            continue

        with open(summary_path, "r", encoding="utf-8") as f:
            payload = json.load(f)

        analysis = payload.get("analysis", {})
        rows.append(
            {
                "video_name": vp.name,
                "processed_frames": payload.get("processed_frames"),
                "elapsed_sec": payload.get("elapsed_sec"),
                "risk_level": analysis.get("risk_level"),
                "risk_score": analysis.get("risk_score"),
                "flags": "|".join(analysis.get("risk_flags", [])),
                "mean_trunk_lean_deg": analysis.get("mean_trunk_lean_deg"),
                "mean_knee_angle_asym_deg": analysis.get("mean_knee_angle_asym_deg"),
                "mean_left_knee_ankle_dev_norm": analysis.get("mean_left_knee_ankle_dev_norm"),
                "mean_right_knee_ankle_dev_norm": analysis.get("mean_right_knee_ankle_dev_norm"),
                "mean_keypoint_conf": analysis.get("mean_keypoint_conf"),
            }
        )

    out_csv = out_root / "batch_summary.csv"
    fieldnames = [
        "video_name",
        "processed_frames",
        "elapsed_sec",
        "risk_level",
        "risk_score",
        "flags",
        "mean_trunk_lean_deg",
        "mean_knee_angle_asym_deg",
        "mean_left_knee_ankle_dev_norm",
        "mean_right_knee_ankle_dev_norm",
        "mean_keypoint_conf",
    ]
    with open(out_csv, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    print(
        f"[OK] total={len(videos)} success={len(rows)} failed={failed} "
        f"summary={out_csv}"
    )
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
