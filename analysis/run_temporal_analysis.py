#!/usr/bin/env python3
"""Stage-A temporal feature extraction for running posture analysis.

Example:
python analysis/run_temporal_analysis.py \
  --cfg ./experiments/vgg19_368x368_sgd.yaml \
  --weight ./network/weight/best_pose.pth \
    --video /path/to/run.mp4 \
  --out-dir /tmp/run_case_001

or use camera:
python analysis/run_temporal_analysis.py \
    --cfg ./experiments/vgg19_368x368_sgd.yaml \
    --weight ./network/weight/best_pose.pth \
    --camera-index 0 --max-seconds 20 \
    --out-dir /tmp/run_case_cam
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import sys
import time
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import cv2
import torch

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from infer.infer_once import load_state_dict_compat, resolve_device, run_model
from lib.config import cfg, update_config
from lib.network.rtpose_vgg import get_model
from lib.utils.common import draw_humans
try:
    from lib.utils.paf_to_pose import paf_to_pose_cpp
except Exception as exc:  # pylint: disable=broad-except
    print(
        "[ERROR] Failed to import paf_to_pose_cpp. "
        "This usually means NumPy 2.x is installed while a compiled extension was built against NumPy 1.x. "
        "Fix by installing 'numpy<2' and rebuilding pafprocess, or reinstalling the project dependencies.",
        file=sys.stderr,
    )
    print(f"[ERROR] import detail: {exc}", file=sys.stderr)
    raise

from analysis.temporal_features import extract_frame_features
from analysis.risk_scoring import load_risk_config, score_risk


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Temporal running analysis")
    parser.add_argument("--cfg", default="./experiments/vgg19_368x368_sgd.yaml", type=str)
    parser.add_argument("--weight", default="./network/weight/best_pose.pth", type=str)
    parser.add_argument("--video", default="", type=str, help="Input video path")
    parser.add_argument("--camera-index", default=-1, type=int, help="Camera device index, >=0 to enable camera mode")
    parser.add_argument("--max-seconds", default=20, type=int, help="Max capture seconds in camera mode")
    parser.add_argument("--out-dir", required=True, type=str)
    parser.add_argument("--device", default="auto", choices=["auto", "cuda", "cpu"])
    parser.add_argument("--preprocess", default="rtpose", choices=["rtpose", "vgg", "inception", "ssd"])
    parser.add_argument("--frame-stride", default=1, type=int, help="Use 1 for all frames, 2 for every 2nd frame")
    parser.add_argument("--save-overlay-video", action="store_true", help="Save skeleton overlay video")
    parser.add_argument("--show-live", action="store_true", help="Show live overlay window while processing")
    parser.add_argument("--preview-image", default="", type=str, help="Optional image path for periodic preview snapshots")
    parser.add_argument("--preview-interval", default=3, type=int, help="Write preview image every N processed frames")
    parser.add_argument("--risk-config", default="", type=str, help="Path to risk config json")
    parser.add_argument(
        "opts",
        nargs=argparse.REMAINDER,
        default=None,
        help="Additional config overrides, passed to update_config",
    )
    return parser.parse_args()


def _pick_primary_human(humans) -> Optional[object]:
    if not humans:
        return None
    # Prefer person with most body parts and highest max-part score.
    def key_fn(h):
        return (h.part_count(), h.get_max_score())
    return max(humans, key=key_fn)


def _extract_parts_px(human, w: int, h: int) -> Dict[int, Tuple[float, float, float]]:
    parts: Dict[int, Tuple[float, float, float]] = {}
    for idx, p in human.body_parts.items():
        parts[idx] = (float(p.x * w), float(p.y * h), float(p.score))
    return parts


def main() -> int:
    args = parse_args()
    update_config(cfg, args)

    video_path = None
    if args.video:
        video_path = (PROJECT_ROOT / args.video).resolve() if not os.path.isabs(args.video) else Path(args.video)
    out_dir = (PROJECT_ROOT / args.out_dir).resolve() if not os.path.isabs(args.out_dir) else Path(args.out_dir)
    weight_path = (PROJECT_ROOT / args.weight).resolve() if not os.path.isabs(args.weight) else Path(args.weight)

    if (video_path is None) and (args.camera_index < 0):
        print("[ERROR] provide --video or set --camera-index >= 0", file=sys.stderr)
        return 2
    if (video_path is not None) and (not video_path.exists()):
        print(f"[ERROR] video not found: {video_path}", file=sys.stderr)
        return 2
    if args.camera_index >= 0 and args.max_seconds <= 0:
        print("[ERROR] --max-seconds must be > 0 in camera mode", file=sys.stderr)
        return 7
    if not weight_path.exists():
        print(f"[ERROR] weight not found: {weight_path}", file=sys.stderr)
        return 3
    if args.frame_stride < 1:
        print("[ERROR] --frame-stride must be >= 1", file=sys.stderr)
        return 4
    if args.preview_interval < 1:
        print("[ERROR] --preview-interval must be >= 1", file=sys.stderr)
        return 9

    risk_config_path = None
    if args.risk_config:
        risk_config_path = (PROJECT_ROOT / args.risk_config).resolve() if not os.path.isabs(args.risk_config) else Path(args.risk_config)
        if not risk_config_path.exists():
            print(f"[ERROR] risk config not found: {risk_config_path}", file=sys.stderr)
            return 6

    out_dir.mkdir(parents=True, exist_ok=True)
    csv_path = out_dir / "features_per_frame.csv"
    summary_path = out_dir / "summary.json"
    overlay_path = out_dir / "overlay.mp4"
    preview_image_path = None
    if args.preview_image:
        preview_image_path = (PROJECT_ROOT / args.preview_image).resolve() if not os.path.isabs(args.preview_image) else Path(args.preview_image)
        preview_image_path.parent.mkdir(parents=True, exist_ok=True)

    device = resolve_device(args.device)

    t0 = time.perf_counter()
    state_dict = load_state_dict_compat(weight_path)
    model = get_model("vgg19")
    model.load_state_dict(state_dict, strict=True)
    model.to(device)
    model.float()
    model.eval()

    source_type = "video"
    source_desc = ""
    if video_path is not None:
        cap = cv2.VideoCapture(str(video_path))
        source_desc = str(video_path)
        if not cap.isOpened():
            print(f"[ERROR] cannot open video: {video_path}", file=sys.stderr)
            return 5
    else:
        source_type = "camera"
        source_desc = f"camera:{args.camera_index}"
        cap = cv2.VideoCapture(args.camera_index, cv2.CAP_V4L2)
        if not cap.isOpened():
            # Fallback probing in case selected index is metadata node.
            cap = None
            for idx in [0, 2, 1, 3, 4, 5, 6, 7, 8]:
                tmp = cv2.VideoCapture(idx, cv2.CAP_V4L2)
                if not tmp.isOpened():
                    tmp.release()
                    continue
                ok_probe, frame_probe = tmp.read()
                if ok_probe and frame_probe is not None and len(frame_probe.shape) >= 2:
                    cap = tmp
                    source_desc = f"camera:{idx}"
                    print(f"[INFO] fallback camera index selected: {idx}")
                    break
                tmp.release()

            if cap is None:
                print(f"[ERROR] cannot open camera index {args.camera_index}", file=sys.stderr)
                return 8

    fps = cap.get(cv2.CAP_PROP_FPS)
    if fps <= 0:
        fps = 30.0

    writer = None
    frame_idx = 0
    processed = 0
    rows: List[Dict[str, Optional[float]]] = []
    capture_t0 = time.perf_counter()

    fieldnames = [
        "frame_idx",
        "timestamp_sec",
        "visible_keypoints",
        "mean_keypoint_conf",
        "trunk_lean_deg",
        "left_knee_angle_deg",
        "right_knee_angle_deg",
        "knee_angle_asym_deg",
        "step_width_px",
        "pelvis_width_px",
        "left_knee_ankle_dev_norm",
        "right_knee_ankle_dev_norm",
        "knee_ankle_dev_asym",
        "frame_w",
        "frame_h",
    ]

    with torch.no_grad(), open(csv_path, "w", newline="", encoding="utf-8") as f:
        csv_writer = csv.DictWriter(f, fieldnames=fieldnames)
        csv_writer.writeheader()

        while True:
            if source_type == "camera" and (time.perf_counter() - capture_t0) >= float(args.max_seconds):
                print(f"[INFO] camera capture reached max_seconds={args.max_seconds}")
                break

            ok, frame = cap.read()
            if not ok:
                break

            if frame_idx % args.frame_stride != 0:
                frame_idx += 1
                continue

            h, w = frame.shape[:2]
            paf, heatmap, _ = run_model(model, frame, args.preprocess, device)
            humans = paf_to_pose_cpp(heatmap, paf, cfg)
            primary = _pick_primary_human(humans)

            if args.save_overlay_video:
                if writer is None:
                    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
                    writer = cv2.VideoWriter(str(overlay_path), fourcc, fps / args.frame_stride, (w, h))
                overlay_frame = draw_humans(frame, humans)
                writer.write(overlay_frame)
            else:
                overlay_frame = draw_humans(frame, humans) if args.show_live else None

            if preview_image_path is not None:
                if overlay_frame is None:
                    overlay_frame = draw_humans(frame, humans)
                if processed % args.preview_interval == 0:
                    cv2.imwrite(str(preview_image_path), overlay_frame)

            if args.show_live:
                if overlay_frame is None:
                    overlay_frame = draw_humans(frame, humans)
                cv2.imshow("Temporal Analysis Live", overlay_frame)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    print("[INFO] live preview interrupted by user")
                    break

            if primary is not None:
                parts = _extract_parts_px(primary, w, h)
                feat = extract_frame_features(parts, w, h)
            else:
                feat = {k: None for k in fieldnames if k not in ("frame_idx", "timestamp_sec")}
                feat["frame_w"] = float(w)
                feat["frame_h"] = float(h)

            feat["frame_idx"] = frame_idx
            feat["timestamp_sec"] = frame_idx / fps

            csv_writer.writerow(feat)
            rows.append(feat)

            processed += 1
            if processed % 20 == 0:
                print(f"[INFO] processed_frames={processed} frame_idx={frame_idx}")

            frame_idx += 1

    cap.release()
    if writer is not None:
        writer.release()
    if args.show_live:
        cv2.destroyAllWindows()

    risk_config = load_risk_config(str(risk_config_path) if risk_config_path else None)
    summary = score_risk(rows, risk_config)
    summary_payload = {
        "source_type": source_type,
        "source": source_desc,
        "video_path": str(video_path) if video_path is not None else None,
        "camera_index": args.camera_index if source_type == "camera" else None,
        "max_seconds": args.max_seconds if source_type == "camera" else None,
        "weight_path": str(weight_path),
        "device": str(device),
        "fps": fps,
        "frame_stride": args.frame_stride,
        "processed_frames": processed,
        "features_csv": str(csv_path),
        "overlay_video": str(overlay_path) if args.save_overlay_video else None,
        "preview_image": str(preview_image_path) if preview_image_path is not None else None,
        "analysis": summary,
        "risk_config": str(risk_config_path) if risk_config_path else "analysis/risk_config.json",
        "elapsed_sec": time.perf_counter() - t0,
    }

    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(summary_payload, f, ensure_ascii=False, indent=2)

    print(f"[OK] summary={summary_path} csv={csv_path} processed_frames={processed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
