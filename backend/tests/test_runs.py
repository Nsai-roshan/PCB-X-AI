from fastapi.testclient import TestClient

from app.main import app

client = TestClient(app)


def test_creates_run_for_sample_circuit() -> None:
    response = client.post("/api/runs", json={"circuitId": "led_resistor"})

    assert response.status_code == 200
    body = response.json()
    assert body["runId"]
    assert body["board"]["width"] == 24
    assert len(body["components"]) == 3
    assert len(body["traces"]) == 3
    assert body["metrics"]["routedNetCount"] == 3
    assert body["events"][0]["type"] == "run_started"
    assert len(body["commentary"]) >= 3
