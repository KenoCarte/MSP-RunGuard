import math
from typing import Dict, Iterable, Optional, Tuple

# OpenPose body index mapping in this repository.
NOSE = 0
NECK = 1
R_SHOULDER = 2
R_ELBOW = 3
R_WRIST = 4
L_SHOULDER = 5
L_ELBOW = 6
L_WRIST = 7
R_HIP = 8
R_KNEE = 9
R_ANKLE = 10
L_HIP = 11
L_KNEE = 12
L_ANKLE = 13

Point = Tuple[float, float]


def _angle_deg(a: Point, b: Point, c: Point) -> Optional[float]:
    # Angle ABC in degrees.
    v1 = (a[0] - b[0], a[1] - b[1])
    v2 = (c[0] - b[0], c[1] - b[1])
    n1 = math.hypot(v1[0], v1[1])
    n2 = math.hypot(v2[0], v2[1])
    if n1 < 1e-6 or n2 < 1e-6:
        return None
    cosv = (v1[0] * v2[0] + v1[1] * v2[1]) / (n1 * n2)
    cosv = max(-1.0, min(1.0, cosv))
    return math.degrees(math.acos(cosv))


def _point(parts: Dict[int, Tuple[float, float, float]], idx: int) -> Optional[Point]:
    if idx not in parts:
        return None
    x, y, _ = parts[idx]
    return x, y


def _mean_point(p1: Point, p2: Point) -> Point:
    return (p1[0] + p2[0]) * 0.5, (p1[1] + p2[1]) * 0.5


def _trunk_lean_deg(neck: Point, mid_hip: Point) -> Optional[float]:
    # Angle between trunk vector (mid_hip -> neck) and vertical up direction.
    v = (neck[0] - mid_hip[0], neck[1] - mid_hip[1])
    n = math.hypot(v[0], v[1])
    if n < 1e-6:
        return None
    vertical = (0.0, -1.0)
    cosv = (v[0] * vertical[0] + v[1] * vertical[1]) / n
    cosv = max(-1.0, min(1.0, cosv))
    return math.degrees(math.acos(cosv))


def extract_frame_features(
    parts: Dict[int, Tuple[float, float, float]],
    width: int,
    height: int,
) -> Dict[str, Optional[float]]:
    out: Dict[str, Optional[float]] = {}

    visible_count = 0
    conf_sum = 0.0
    for _, (_, _, s) in parts.items():
        visible_count += 1
        conf_sum += s

    out["visible_keypoints"] = float(visible_count)
    out["mean_keypoint_conf"] = (conf_sum / visible_count) if visible_count > 0 else None

    neck = _point(parts, NECK)
    lhip = _point(parts, L_HIP)
    rhip = _point(parts, R_HIP)
    lknee = _point(parts, L_KNEE)
    rknee = _point(parts, R_KNEE)
    lankle = _point(parts, L_ANKLE)
    rankle = _point(parts, R_ANKLE)

    mid_hip = _mean_point(lhip, rhip) if lhip and rhip else None
    out["trunk_lean_deg"] = _trunk_lean_deg(neck, mid_hip) if neck and mid_hip else None

    out["left_knee_angle_deg"] = _angle_deg(lhip, lknee, lankle) if lhip and lknee and lankle else None
    out["right_knee_angle_deg"] = _angle_deg(rhip, rknee, rankle) if rhip and rknee and rankle else None

    if out["left_knee_angle_deg"] is not None and out["right_knee_angle_deg"] is not None:
        out["knee_angle_asym_deg"] = abs(out["left_knee_angle_deg"] - out["right_knee_angle_deg"])
    else:
        out["knee_angle_asym_deg"] = None

    if lankle and rankle:
        out["step_width_px"] = abs(lankle[0] - rankle[0])
    else:
        out["step_width_px"] = None

    if lhip and rhip:
        pelvis_width_px = abs(lhip[0] - rhip[0])
    else:
        pelvis_width_px = None
    out["pelvis_width_px"] = pelvis_width_px

    # Knee-to-ankle horizontal deviation, normalized by pelvis width.
    if lknee and lankle and pelvis_width_px and pelvis_width_px > 1e-6:
        out["left_knee_ankle_dev_norm"] = abs(lknee[0] - lankle[0]) / pelvis_width_px
    else:
        out["left_knee_ankle_dev_norm"] = None

    if rknee and rankle and pelvis_width_px and pelvis_width_px > 1e-6:
        out["right_knee_ankle_dev_norm"] = abs(rknee[0] - rankle[0]) / pelvis_width_px
    else:
        out["right_knee_ankle_dev_norm"] = None

    if out["left_knee_ankle_dev_norm"] is not None and out["right_knee_ankle_dev_norm"] is not None:
        out["knee_ankle_dev_asym"] = abs(out["left_knee_ankle_dev_norm"] - out["right_knee_ankle_dev_norm"])
    else:
        out["knee_ankle_dev_asym"] = None

    out["frame_w"] = float(width)
    out["frame_h"] = float(height)
    return out


def safe_mean(values: Iterable[Optional[float]]) -> Optional[float]:
    arr = [v for v in values if v is not None]
    if not arr:
        return None
    return sum(arr) / len(arr)


def build_risk_summary(feature_rows: Iterable[Dict[str, Optional[float]]]) -> Dict[str, object]:
    rows = list(feature_rows)
    if not rows:
        return {
            "valid_frames": 0,
            "risk_flags": [],
            "message": "no valid frame features",
        }

    mean_trunk = safe_mean(r.get("trunk_lean_deg") for r in rows)
    mean_asym = safe_mean(r.get("knee_angle_asym_deg") for r in rows)
    mean_dev_l = safe_mean(r.get("left_knee_ankle_dev_norm") for r in rows)
    mean_dev_r = safe_mean(r.get("right_knee_ankle_dev_norm") for r in rows)
    mean_conf = safe_mean(r.get("mean_keypoint_conf") for r in rows)

    flags = []
    # Conservative rule thresholds for first-stage screening.
    if mean_trunk is not None and mean_trunk > 22.0:
        flags.append("forward_trunk_lean_risk")
    if mean_asym is not None and mean_asym > 15.0:
        flags.append("left_right_knee_asymmetry_risk")
    if mean_dev_l is not None and mean_dev_l > 0.38:
        flags.append("left_knee_alignment_risk")
    if mean_dev_r is not None and mean_dev_r > 0.38:
        flags.append("right_knee_alignment_risk")
    if mean_conf is not None and mean_conf < 0.2:
        flags.append("low_pose_confidence")

    return {
        "valid_frames": len(rows),
        "mean_trunk_lean_deg": mean_trunk,
        "mean_knee_angle_asym_deg": mean_asym,
        "mean_left_knee_ankle_dev_norm": mean_dev_l,
        "mean_right_knee_ankle_dev_norm": mean_dev_r,
        "mean_keypoint_conf": mean_conf,
        "risk_flags": flags,
        "risk_level": "high" if len(flags) >= 3 else ("medium" if len(flags) >= 1 else "low"),
    }
