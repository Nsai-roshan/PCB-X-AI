from __future__ import annotations

import os
from typing import Any

import httpx

_TRACEMIND_SYSTEM = """You are an expert explaining TraceMind, a simplified PCB design automation engine.

CRITICAL RULES — never break these:
- TraceMind uses a UNITLESS GRID (grid cells), NOT millimetres, mils, or any real-world unit.
- There are NO vias, NO multi-layer routing, NO copper pours, NO footprints in any standard sense.
- Trace WIDTH is not configurable per-net. Resistance is controlled by `traceResistancePerCell` (ohms per cell).
- The IR-drop solver is a sparse CG solver on a conductance matrix built from routed trace cells.
- The thermal solver is a 2-D Gauss-Seidel FDM grid (not ANSYS, not FEA, not 3-D).
- The placer uses Leiden community detection followed by simulated annealing.
- The router is a BFS maze router that connects the first and last pin of each net only.

Parameters the user can change (use exact names):
  traceResistancePerCell  — ohms per grid cell; lower = thicker traces, less IR drop
  loadCurrentPerPinA      — amps per load pin; higher = heavier loads, more IR drop
  violationThresholdMv    — mV above which a cell is flagged as a violation
  supplyVoltage           — VCC rail voltage in volts
  mcuPowerW               — watts per MCU/IC/SoC component (drives thermal)
  ledPowerW               — watts per LED component
  defaultPowerW           — watts per resistor/capacitor/other

When giving fix suggestions:
- ALWAYS reference the current value vs the default value if the user changed a param.
- Be specific: say "lower traceResistancePerCell from X to Y" not "lower it".
- NEVER suggest vias, mm values, trace width in mm, copper pours, or grid resolution in mm.
- If a param is already at a sensible default and the problem is caused by another param being
  cranked high, say so clearly.
"""


def _groq_post(prompt: str) -> str | None:
    api_key = os.getenv("GROQ_API_KEY")
    if not api_key:
        return None
    model    = os.getenv("GROQ_MODEL", "llama-3.3-70b-versatile")
    base_url = os.getenv("GROQ_BASE_URL", "https://api.groq.com/openai/v1")
    try:
        with httpx.Client(timeout=30.0) as client:
            resp = client.post(
                f"{base_url}/chat/completions",
                headers={"Authorization": f"Bearer {api_key}"},
                json={
                    "model": model,
                    "messages": [
                        {"role": "system", "content": _TRACEMIND_SYSTEM},
                        {"role": "user",   "content": prompt},
                    ],
                    "temperature": 0.3,
                },
            )
            resp.raise_for_status()
            return (
                resp.json()
                .get("choices", [{}])[0]
                .get("message", {})
                .get("content", "")
            )
    except Exception:
        return None


def enhance_commentary(
    circuit_id: str,
    metrics: dict[str, Any],
    commentary: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    prompt = (
        f"Circuit: {circuit_id}\nMetrics: {metrics}\nBase commentary: {commentary}\n\n"
        "Rewrite into short, clear status updates for a beginner. "
        "One line per item, prefixed with the stage name e.g. 'placement: ...'."
    )
    content = _groq_post(prompt)
    if content is None:
        return [{**item, "source": "local"} for item in commentary]
    lines = [ln.strip("- ").strip() for ln in content.splitlines() if ln.strip()]
    rewritten: list[dict[str, Any]] = []
    for i, item in enumerate(commentary):
        rewritten.append({**item, "text": lines[i] if i < len(lines) else item["text"], "source": "groq"})
    return rewritten


def explain_circuit(
    circuit_id: str,
    circuit_json: dict[str, Any],
    metrics: dict[str, Any] | None = None,
) -> str:
    components   = circuit_json.get("components", [])
    nets         = circuit_json.get("nets", [])
    comp_summary = ", ".join(f"{c['id']} ({c.get('kind','?')})" for c in components)
    net_summary  = ", ".join(n["id"] for n in nets)

    if metrics:
        max_ir   = metrics.get("maxIrDropMv", 0)
        ir_viols = metrics.get("irViolationCount", 0)
        max_temp = metrics.get("maxTemperatureC", 25)
        unrouted = metrics.get("unroutedNetCount", 0)
        util     = metrics.get("boardUtilizationPercent", 0)
        results_block = (
            f"\nTraceMind run results:\n"
            f"- IR drop max: {max_ir:.1f} mV ({ir_viols} violation(s) above threshold)\n"
            f"- Peak temperature: {max_temp:.1f} degC (FDM solver)\n"
            f"- Unrouted nets: {unrouted}\n"
            f"- Board utilisation: {util:.1f}%\n"
        )
        prompt = (
            f"TraceMind circuit '{circuit_id}': components: {comp_summary}. Nets: {net_summary}.\n"
            f"{results_block}\n"
            "Answer in 4 short paragraphs:\n"
            "1. What does this circuit do?\n"
            "2. What does each component type contribute?\n"
            "3. Analyse the results: IR drop, likely hotspot locations, routing quality.\n"
            "4. What specific TraceMind parameters should the user adjust to improve this? "
            "Use exact parameter names and suggest specific values."
        )
    else:
        prompt = (
            f"TraceMind circuit '{circuit_id}': components: {comp_summary}. Nets: {net_summary}.\n\n"
            "Answer in 3 short paragraphs:\n"
            "1. What does this circuit do?\n"
            "2. What does each component type contribute?\n"
            "3. Which TraceMind parameters would be most interesting to experiment with?"
        )

    result = _groq_post(prompt)
    if result is None:
        base = f"Circuit '{circuit_id}': {len(components)} components, {len(nets)} nets."
        if metrics:
            base += f" IR: {metrics.get('maxIrDropMv',0):.1f} mV, temp: {metrics.get('maxTemperatureC',25):.1f} degC."
        return base + " (Set GROQ_API_KEY to enable AI analysis.)"
    return result


def suggest_fixes(
    circuit_id: str,
    metrics: dict[str, Any],
    drc_violations: int,
    ir_violations: int,
    run_params: dict[str, Any] | None = None,
) -> list[str]:
    if drc_violations == 0 and ir_violations == 0:
        return []

    issues: list[str] = []
    if drc_violations:
        issues.append(f"{drc_violations} unrouted net(s) — BFS router found no path")
    if ir_violations:
        issues.append(
            f"{ir_violations} IR-drop violation(s) — max drop "
            f"{metrics.get('maxIrDropMv', 0):.1f} mV"
        )

    # Build a clear picture of which params are non-default
    param_lines: list[str] = []
    if run_params:
        for k, info in run_params.items():
            cur = info.get("current")
            dflt = info.get("default")
            changed = info.get("changed", False)
            if changed:
                param_lines.append(f"  {k}: currently {cur} (default is {dflt}) — USER CHANGED THIS")
            else:
                param_lines.append(f"  {k}: {cur} (at default)")

    params_section = ""
    if param_lines:
        params_section = "\nCurrent parameter values:\n" + "\n".join(param_lines)

    prompt = (
        f"TraceMind circuit '{circuit_id}' has these issues:\n"
        + "\n".join(f"- {i}" for i in issues)
        + f"\nMetrics: {metrics}"
        + params_section
        + "\n\nGive 3-5 fix suggestions. For each:"
        + "\n- State the exact parameter name or JSON field to change"
        + "\n- Give the specific current value and the specific value to try"
        + "\n- Explain in one sentence WHY that change fixes the issue"
        + "\nIf a problem is caused by a non-default parameter being set too high/low, say so directly."
        + "\nNever suggest vias, mm values, or things that don't exist in TraceMind."
    )
    content = _groq_post(prompt)
    if content is None:
        defaults: list[str] = []
        if drc_violations:
            defaults.append("Increase board width/height in the circuit JSON to give the BFS router more space.")
        if ir_violations:
            # Check if resistance is cranked high
            r_info = (run_params or {}).get("traceResistancePerCell", {})
            if r_info.get("changed") and r_info.get("current", 0) > r_info.get("default", 0.05):
                defaults.append(
                    f"Lower traceResistancePerCell from {r_info['current']} back toward "
                    f"{r_info['default']} — it's currently set much higher than the default, "
                    "which simulates very thin traces and causes high IR drop."
                )
            else:
                defaults.append("Lower traceResistancePerCell (e.g. 0.01) to simulate thicker power traces.")
        return defaults

    bullets = [
        ln.lstrip("-•123456789. ").strip()
        for ln in content.splitlines()
        if ln.strip() and (ln.strip()[0] in "-•" or ln.strip()[0].isdigit())
    ]
    return bullets or [content.strip()]