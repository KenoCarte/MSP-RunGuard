#!/usr/bin/env python3
"""Single-image inference entry for OpenPose project.

Usage example:
python infer/infer_once.py \
  --cfg ./experiments/vgg19_368x368_sgd.yaml \
  --weight ./network/weight/best_pose.pth \
  --input ./readme/ski.jpg \
  --output /tmp/pose_out.jpg \
  --device auto
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict, List

import cv2
import numpy as np
import torch
from collections import OrderedDict

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from lib.config import cfg, update_config
from lib.datasets.preprocessing import (
    inception_preprocess,
    rtpose_preprocess,
    ssd_preprocess,
    vgg_preprocess,
)
from lib.network import im_transform
from lib.network.rtpose_vgg import get_model
from lib.utils.common import draw_humans
from lib.utils.paf_to_pose import paf_to_pose_cpp


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Single image pose inference")
    parser.add_argument(
        "--cfg",
        default="./experiments/vgg19_368x368_sgd.yaml",
        type=str,
        help="Path to config yaml",
    )
    parser.add_argument(
        "--weight",
        default="./network/weight/best_pose.pth",
        type=str,
        help="Path to model checkpoint (.pth)",
    )
    parser.add_argument("--input", required=True, type=str, help="Input image path")
    parser.add_argument("--output", required=True, type=str, help="Output image path")
    parser.add_argument(
        "--device",
        default="auto",
        choices=["auto", "cuda", "cpu"],
        help="Inference device",
    )
    parser.add_argument(
        "--preprocess",
        default="rtpose",
        choices=["rtpose", "vgg", "inception", "ssd"],
        help="Image preprocess mode",
    )
    parser.add_argument(
        "--json",
        default="",
        type=str,
        help="Optional path to save keypoints json",
    )
    parser.add_argument(
        "opts",
        nargs=argparse.REMAINDER,
        default=None,
        help="Additional config overrides, passed to update_config",
    )
    return parser.parse_args()


def resolve_device(device_arg: str) -> torch.device:
    if device_arg == "cuda":
        if not torch.cuda.is_available():
            raise RuntimeError("CUDA requested but not available")
        return torch.device("cuda")
    if device_arg == "cpu":
        return torch.device("cpu")
    return torch.device("cuda" if torch.cuda.is_available() else "cpu")


def load_state_dict_compat(weight_path: Path) -> Dict[str, torch.Tensor]:
    ckpt = torch.load(str(weight_path), map_location="cpu")
    if isinstance(ckpt, dict) and "state_dict" in ckpt:
        state_dict = ckpt["state_dict"]
    elif isinstance(ckpt, dict):
        state_dict = ckpt
    else:
        raise TypeError(f"Unexpected checkpoint type: {type(ckpt)}")

    # Strip DataParallel prefix if present.
    normalized = OrderedDict()
    for k, v in state_dict.items():
        nk = k.replace("module.", "", 1) if k.startswith("module.") else k
        normalized[nk] = v
    return normalized


def preprocess_image(img_bgr: np.ndarray, preprocess: str) -> tuple[np.ndarray, float]:
    inp_size = cfg.DATASET.IMAGE_SIZE
    img_cropped, im_scale, _ = im_transform.crop_with_factor(
        img_bgr, inp_size, factor=cfg.MODEL.DOWNSAMPLE, is_ceil=True
    )

    if preprocess == "rtpose":
        im_data = rtpose_preprocess(img_cropped)
    elif preprocess == "vgg":
        im_data = vgg_preprocess(img_cropped)
    elif preprocess == "inception":
        im_data = inception_preprocess(img_cropped)
    else:
        im_data = ssd_preprocess(img_cropped)

    return np.expand_dims(im_data, 0), im_scale


def run_model(
    model: torch.nn.Module,
    img_bgr: np.ndarray,
    preprocess: str,
    device: torch.device,
) -> tuple[np.ndarray, np.ndarray, float]:
    batch_images, im_scale = preprocess_image(img_bgr, preprocess)
    batch_tensor = torch.from_numpy(batch_images).to(device=device, dtype=torch.float32)

    with torch.no_grad():
        predicted_outputs, _ = model(batch_tensor)
        output1, output2 = predicted_outputs[-2], predicted_outputs[-1]

    heatmap = output2.detach().cpu().numpy().transpose(0, 2, 3, 1)[0]
    paf = output1.detach().cpu().numpy().transpose(0, 2, 3, 1)[0]
    return paf, heatmap, im_scale


def humans_to_jsonable(humans: List[Any]) -> List[Dict[str, Any]]:
    result: List[Dict[str, Any]] = []
    for hidx, human in enumerate(humans):
        parts: Dict[str, Dict[str, float]] = {}
        for pid, part in human.body_parts.items():
            parts[str(pid)] = {
                "x": float(part.x),
                "y": float(part.y),
                "score": float(part.score),
            }
        result.append({"id": hidx, "parts": parts})
    return result


def main() -> int:
    args = parse_args()

    # Keep config behavior aligned with the original project scripts.
    update_config(cfg, args)

    input_path = (PROJECT_ROOT / args.input).resolve() if not os.path.isabs(args.input) else Path(args.input)
    output_path = (PROJECT_ROOT / args.output).resolve() if not os.path.isabs(args.output) else Path(args.output)
    weight_path = (PROJECT_ROOT / args.weight).resolve() if not os.path.isabs(args.weight) else Path(args.weight)

    if not input_path.exists():
        print(f"[ERROR] Input image not found: {input_path}", file=sys.stderr)
        return 2
    if not weight_path.exists():
        print(f"[ERROR] Weight file not found: {weight_path}", file=sys.stderr)
        return 3

    device = resolve_device(args.device)
    t0 = time.perf_counter()

    try:
        state_dict = load_state_dict_compat(weight_path)
        model = get_model("vgg19")
        model.load_state_dict(state_dict, strict=True)
        model.to(device)
        model.float()
        model.eval()

        img = cv2.imread(str(input_path), cv2.IMREAD_COLOR)
        if img is None:
            print(f"[ERROR] Failed to read image: {input_path}", file=sys.stderr)
            return 4

        paf, heatmap, _ = run_model(model, img, args.preprocess, device)
        humans = paf_to_pose_cpp(heatmap, paf, cfg)
        rendered = draw_humans(img, humans)

        output_path.parent.mkdir(parents=True, exist_ok=True)
        ok = cv2.imwrite(str(output_path), rendered)
        if not ok:
            print(f"[ERROR] Failed to write image: {output_path}", file=sys.stderr)
            return 5

        if args.json:
            json_path = (PROJECT_ROOT / args.json).resolve() if not os.path.isabs(args.json) else Path(args.json)
            json_path.parent.mkdir(parents=True, exist_ok=True)
            payload = {
                "input": str(input_path),
                "output": str(output_path),
                "device": str(device),
                "num_humans": len(humans),
                "humans": humans_to_jsonable(humans),
            }
            with open(json_path, "w", encoding="utf-8") as f:
                json.dump(payload, f, ensure_ascii=False, indent=2)

        elapsed = (time.perf_counter() - t0) * 1000.0
        print(
            f"[OK] output={output_path} humans={len(humans)} "
            f"device={device} cost_ms={elapsed:.2f}"
        )
        return 0

    except Exception as exc:  # pylint: disable=broad-except
        print(f"[ERROR] Inference failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
