import json
from pathlib import Path
from typing import Dict, Iterable, List, Optional

from analysis.temporal_features import safe_mean

DEFAULT_CONFIG_PATH = Path(__file__).resolve().parent / "risk_config.json"


def load_risk_config(config_path: Optional[str] = None) -> Dict[str, object]:
    path = Path(config_path).resolve() if config_path else DEFAULT_CONFIG_PATH
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def _collect_means(feature_rows: Iterable[Dict[str, Optional[float]]]) -> Dict[str, Optional[float]]:
    rows = list(feature_rows)
    return {
        "mean_trunk_lean_deg": safe_mean(r.get("trunk_lean_deg") for r in rows),
        "mean_knee_angle_asym_deg": safe_mean(r.get("knee_angle_asym_deg") for r in rows),
        "mean_left_knee_ankle_dev_norm": safe_mean(r.get("left_knee_ankle_dev_norm") for r in rows),
        "mean_right_knee_ankle_dev_norm": safe_mean(r.get("right_knee_ankle_dev_norm") for r in rows),
        "mean_keypoint_conf": safe_mean(r.get("mean_keypoint_conf") for r in rows),
    }


def score_risk(feature_rows: Iterable[Dict[str, Optional[float]]], config: Dict[str, object]) -> Dict[str, object]:
    rows = list(feature_rows)
    if not rows:
        return {
            "valid_frames": 0,
            "risk_flags": [],
            "risk_score": 0.0,
            "risk_level": "unknown",
            "advice": ["No valid frame features. Risk cannot be assessed."],
            "explainability": {
                "triggered_rules": [],
                "score_contributions": [],
                "metric_checks": [],
            },
        }

    means = _collect_means(rows)
    thresholds = config.get("thresholds", {})
    weights = config.get("weights", {})
    advice_map = config.get("advice", {})
    level_cfg = config.get("risk_level", {})

    flags: List[str] = []

    trunk_thr = float(thresholds.get("forward_trunk_lean_deg", 22.0))
    asym_thr = float(thresholds.get("knee_angle_asym_deg", 15.0))
    left_dev_thr = float(thresholds.get("left_knee_ankle_dev_norm", 0.38))
    right_dev_thr = float(thresholds.get("right_knee_ankle_dev_norm", 0.38))
    min_conf = float(thresholds.get("min_mean_keypoint_conf", 0.2))

    trunk_triggered = means["mean_trunk_lean_deg"] is not None and means["mean_trunk_lean_deg"] > trunk_thr
    asym_triggered = means["mean_knee_angle_asym_deg"] is not None and means["mean_knee_angle_asym_deg"] > asym_thr
    left_triggered = means["mean_left_knee_ankle_dev_norm"] is not None and means["mean_left_knee_ankle_dev_norm"] > left_dev_thr
    right_triggered = means["mean_right_knee_ankle_dev_norm"] is not None and means["mean_right_knee_ankle_dev_norm"] > right_dev_thr
    conf_triggered = means["mean_keypoint_conf"] is not None and means["mean_keypoint_conf"] < min_conf

    if trunk_triggered:
        flags.append("forward_trunk_lean_risk")
    if asym_triggered:
        flags.append("left_right_knee_asymmetry_risk")
    if left_triggered:
        flags.append("left_knee_alignment_risk")
    if right_triggered:
        flags.append("right_knee_alignment_risk")
    if conf_triggered:
        flags.append("low_pose_confidence")

    score = 0.0
    contributions: List[Dict[str, object]] = []
    for f in flags:
        weight_val = float(weights.get(f, 0.1))
        score += weight_val
        contributions.append({
            "flag": f,
            "weight": weight_val,
        })
    score = min(1.0, score)

    high_score = float(level_cfg.get("high_score", 0.60))
    medium_score = float(level_cfg.get("medium_score", 0.25))
    if score >= high_score:
        level = "high"
    elif score >= medium_score:
        level = "medium"
    else:
        level = "low"

    advice = [str(advice_map.get(f, f)) for f in flags]
    if not advice:
        advice = ["Current risk is low. Keep training and continue monitoring."]

    metric_checks = [
        {
            "metric": "mean_trunk_lean_deg",
            "value": means["mean_trunk_lean_deg"],
            "threshold": trunk_thr,
            "relation": ">",
            "triggered": trunk_triggered,
            "flag": "forward_trunk_lean_risk",
        },
        {
            "metric": "mean_knee_angle_asym_deg",
            "value": means["mean_knee_angle_asym_deg"],
            "threshold": asym_thr,
            "relation": ">",
            "triggered": asym_triggered,
            "flag": "left_right_knee_asymmetry_risk",
        },
        {
            "metric": "mean_left_knee_ankle_dev_norm",
            "value": means["mean_left_knee_ankle_dev_norm"],
            "threshold": left_dev_thr,
            "relation": ">",
            "triggered": left_triggered,
            "flag": "left_knee_alignment_risk",
        },
        {
            "metric": "mean_right_knee_ankle_dev_norm",
            "value": means["mean_right_knee_ankle_dev_norm"],
            "threshold": right_dev_thr,
            "relation": ">",
            "triggered": right_triggered,
            "flag": "right_knee_alignment_risk",
        },
        {
            "metric": "mean_keypoint_conf",
            "value": means["mean_keypoint_conf"],
            "threshold": min_conf,
            "relation": "<",
            "triggered": conf_triggered,
            "flag": "low_pose_confidence",
        },
    ]

    return {
        "valid_frames": len(rows),
        **means,
        "risk_flags": flags,
        "risk_score": score,
        "risk_level": level,
        "advice": advice,
        "explainability": {
            "triggered_rules": flags,
            "score_contributions": contributions,
            "metric_checks": metric_checks,
        },
    }
