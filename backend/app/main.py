import os
from pathlib import Path
from typing import Any

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

from app.services.circuit_loader import get_circuit_path, list_circuits, load_circuit
from app.services.engine_runner import run_circuit
from app.services.ai_narrator import explain_circuit


def _load_dotenv() -> None:
    """Load backend/.env into os.environ without overriding existing vars."""
    env_file = Path(__file__).resolve().parents[1] / ".env"
    if not env_file.exists():
        return
    for line in env_file.read_text(encoding="utf-8-sig").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        key = key.strip()
        value = value.strip()
        if key and key not in os.environ:
            os.environ[key] = value


_load_dotenv()

app = FastAPI(title="TraceMind API")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://127.0.0.1:5173", "http://localhost:5173"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


class RunParams(BaseModel):
    traceResistancePerCell: float = 0.05
    loadCurrentPerPinA: float     = 0.01
    violationThresholdMv: float   = 30.0
    supplyVoltage: float          = 3.3
    mcuPowerW: float              = 0.15
    ledPowerW: float              = 0.05
    defaultPowerW: float          = 0.02


class RunRequest(BaseModel):
    circuitId: str
    params: RunParams = RunParams()


class ExplainRequest(BaseModel):
    metrics: dict[str, Any] | None = None


@app.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok"}


@app.get("/api/circuits")
def circuits() -> dict[str, list[dict[str, str]]]:
    return {"circuits": list_circuits()}


@app.post("/api/circuits/{circuit_id}/explain")
def explain(circuit_id: str, body: ExplainRequest = ExplainRequest()) -> dict[str, str]:
    try:
        circuit_json = load_circuit(circuit_id)
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail="Circuit not found") from exc
    return {"explanation": explain_circuit(circuit_id, circuit_json, body.metrics)}


@app.post("/api/runs")
def create_run(request: RunRequest) -> dict:
    try:
        load_circuit(request.circuitId)
        circuit_path = get_circuit_path(request.circuitId)
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail="Circuit not found") from exc
    try:
        return run_circuit(request.circuitId, circuit_path, request.params.model_dump())
    except RuntimeError as exc:
        raise HTTPException(status_code=500, detail=str(exc)) from exc
