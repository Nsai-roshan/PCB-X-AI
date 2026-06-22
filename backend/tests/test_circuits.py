from fastapi.testclient import TestClient

from app.main import app

client = TestClient(app)


def test_lists_sample_circuits() -> None:
    response = client.get("/api/circuits")

    assert response.status_code == 200
    body = response.json()
    assert len(body["circuits"]) >= 1
    assert body["circuits"][0]["id"] == "led_resistor"
