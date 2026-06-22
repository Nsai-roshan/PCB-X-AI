from __future__ import annotations

import json
import os
import site
import subprocess
import uuid
from pathlib import Path
from typing import Any

from app.services.ai_narrator import enhance_commentary, suggest_fixes


ROOT_DIR = Path(__file__).resolve().parents[3]
CORE_DIR = ROOT_DIR / "core"
BUILD_DIR = CORE_DIR / "build"


def _engine_candidates() -> list[Path]:
    return [
        BUILD_DIR / "Debug" / "tracemind_engine.exe",
        BUILD_DIR / "Release" / "tracemind_engine.exe",
        BUILD_DIR / "tracemind_engine.exe",
        BUILD_DIR / "tracemind_engine",
    ]


def _build_environment() -> dict[str, str]:
    env = os.environ.copy()
    user_scripts = Path(site.getusersitepackages()).parent / "Scripts"
    if user_scripts.exists():
        env["PATH"] = str(user_scripts) + os.pathsep + env.get("PATH", "")
    return env


def _find_engine_binary() -> Path | None:
    for candidate in _engine_candidates():
        if candidate.exists():
            return candidate
    return None


def _ensure_engine_built() -> Path:
    binary = _find_engine_binary()
    if binary is not None:
        return binary

    env = _build_environment()
    configure = subprocess.run(
        ["cmake", "-S", "core", "-B", "core/build"],
        cwd=ROOT_DIR, env=env, capture_output=True, text=True, check=False,
    )
    if configure.returncode != 0:
        raise RuntimeError(configure.stderr or configure.stdout)

    build = subprocess.run(
        ["cmake", "--build", "core/build", "--config", "Debug"],
        cwd=ROOT_DIR, env=env, capture_output=True, text=True, check=False,
    )
    if build.returncode != 0:
        raise RuntimeError(build.stderr or build.stdout)

    binary = _find_engine_binary()
    if binary is None:
        raise RuntimeError("Engine binary was not produced by the build step.")
    return binary


# Default param values for comparison in AI context
_DEFAULTS = {
    "traceResistancePerCell": 0.05,
    "loadCurrentPerPinA":     0.01,
    "violationThresholdMv":   30.0,
    "supplyVoltage":          3.3,
    "mcuPowerW":              0.15,
    "ledPowerW":              0.05,
    "defaultPowerW":          0.02,
}


def run_circuit(circuit_id: str, circuit_path: Path,
                params: dict[str, Any] | None = None) -> dict:
    binary = _ensure_engine_built()
    cmd = [str(binary), str(circuit_path)]
    if params:
        cmd.append(json.dumps(params))

    result = subprocess.run(
        cmd, cwd=ROOT_DIR, capture_output=True, text=True, check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr or result.stdout)

    payload      = json.loads(result.stdout)
    run_id       = str(uuid.uuid4())
    payload["runId"]     = run_id
    payload["circuitId"] = circuit_id

    for event in payload.get("events", []):
        event["runId"] = run_id

    metrics = payload.get("metrics", {})
    payload["commentary"] = enhance_commentary(circuit_id, metrics,
                                               payload.get("commentary", []))

    # Build a human-readable param context so the AI knows current vs default values
    param_context: dict[str, Any] = {}
    if params:
        for k, current_val in params.items():
            default_val = _DEFAULTS.get(k)
            param_context[k] = {
                "current": current_val,
                "default": default_val,
                "changed": default_val is not None and abs(current_val - default_val) > 1e-9,
            }

    payload["fixSuggestions"] = suggest_fixes(
        circuit_id, metrics,
        drc_violations=metrics.get("drcViolationCount", 0),
        ir_violations =metrics.get("irViolationCount",  0),
        run_params=param_context,
    )
    return payload