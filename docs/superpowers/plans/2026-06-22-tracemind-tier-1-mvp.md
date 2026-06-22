# TraceMind Tier 1 MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first end-to-end TraceMind MVP with a tested C++ core for circuit parsing and placement, a FastAPI backend that runs the engine and streams events, and a React frontend that visualizes a sample run.

**Architecture:** The repo is split into a C++ engine, a Python orchestration layer, and a React dashboard. The first implementation keeps the engine contract JSON-over-stdout so placement and routing logic can evolve independently from the backend and frontend.

**Tech Stack:** C++17, CMake, GoogleTest, nlohmann/json, Python 3.11+, FastAPI, pytest, React, TypeScript, Vite, Vitest

---

### Task 1: Scaffold the workspace and sample data

**Files:**
- Create: `core/CMakeLists.txt`
- Create: `core/src/main.cpp`
- Create: `core/src/model/circuit.hpp`
- Create: `core/src/model/circuit.cpp`
- Create: `core/tests/circuit_parse_test.cpp`
- Create: `backend/requirements.txt`
- Create: `backend/app/main.py`
- Create: `frontend/package.json`
- Create: `frontend/tsconfig.json`
- Create: `frontend/vite.config.ts`
- Create: `frontend/index.html`
- Create: `frontend/src/main.tsx`
- Create: `frontend/src/App.tsx`
- Create: `frontend/src/styles.css`
- Create: `data/sample_circuits/led_resistor.json`
- Modify: `tasks/todo.md`

- [ ] **Step 1: Write the failing C++ parse test**

```cpp
#include "model/circuit.hpp"

#include <gtest/gtest.h>

TEST(CircuitParseTest, ParsesBasicCircuitJson) {
  const auto circuit = tracemind::model::ParseCircuit(R"json(
  {
    "board": { "width": 20, "height": 12 },
    "components": [
      {
        "id": "BAT1",
        "kind": "battery",
        "width": 3,
        "height": 2,
        "pins": [{ "id": "POS", "x": 2, "y": 0 }, { "id": "NEG", "x": 0, "y": 0 }]
      }
    ],
    "nets": [
      { "id": "VCC", "connections": [{ "component": "BAT1", "pin": "POS" }] }
    ]
  }
  )json");

  EXPECT_EQ(circuit.board_width, 20);
  EXPECT_EQ(circuit.board_height, 12);
  ASSERT_EQ(circuit.components.size(), 1u);
  EXPECT_EQ(circuit.components[0].id, "BAT1");
  ASSERT_EQ(circuit.nets.size(), 1u);
  EXPECT_EQ(circuit.nets[0].id, "VCC");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake -S core -B core/build && cmake --build core/build && ctest --test-dir core/build --output-on-failure`
Expected: FAIL because `ParseCircuit` and model types do not exist yet.

- [ ] **Step 3: Write the minimal engine scaffold and parser**

```cpp
// core/src/model/circuit.hpp
#pragma once

#include <string>
#include <vector>

namespace tracemind::model {

struct Pin {
  std::string id;
  int x;
  int y;
};

struct Component {
  std::string id;
  std::string kind;
  int width;
  int height;
  std::vector<Pin> pins;
};

struct Connection {
  std::string component;
  std::string pin;
};

struct Net {
  std::string id;
  std::vector<Connection> connections;
};

struct Circuit {
  int board_width;
  int board_height;
  std::vector<Component> components;
  std::vector<Net> nets;
};

Circuit ParseCircuit(const std::string& json_text);

}  // namespace tracemind::model
```

```cpp
// core/src/model/circuit.cpp
#include "model/circuit.hpp"

#include <nlohmann/json.hpp>

namespace tracemind::model {
namespace {
using json = nlohmann::json;
}

Circuit ParseCircuit(const std::string& json_text) {
  const auto parsed = json::parse(json_text);

  Circuit circuit{
      .board_width = parsed.at("board").at("width").get<int>(),
      .board_height = parsed.at("board").at("height").get<int>(),
  };

  for (const auto& component_json : parsed.at("components")) {
    Component component{
        .id = component_json.at("id").get<std::string>(),
        .kind = component_json.at("kind").get<std::string>(),
        .width = component_json.at("width").get<int>(),
        .height = component_json.at("height").get<int>(),
    };

    for (const auto& pin_json : component_json.at("pins")) {
      component.pins.push_back(Pin{
          .id = pin_json.at("id").get<std::string>(),
          .x = pin_json.at("x").get<int>(),
          .y = pin_json.at("y").get<int>(),
      });
    }

    circuit.components.push_back(component);
  }

  for (const auto& net_json : parsed.at("nets")) {
    Net net{.id = net_json.at("id").get<std::string>()};

    for (const auto& connection_json : net_json.at("connections")) {
      net.connections.push_back(Connection{
          .component = connection_json.at("component").get<std::string>(),
          .pin = connection_json.at("pin").get<std::string>(),
      });
    }

    circuit.nets.push_back(net);
  }

  return circuit;
}

}  // namespace tracemind::model
```

```cpp
// core/src/main.cpp
int main() { return 0; }
```

```cmake
# core/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(tracemind_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

FetchContent_MakeAvailable(googletest json)

enable_testing()

add_library(tracemind_model
  src/model/circuit.cpp
)
target_include_directories(tracemind_model PUBLIC src)
target_link_libraries(tracemind_model PUBLIC nlohmann_json::nlohmann_json)

add_executable(tracemind_engine src/main.cpp)
target_link_libraries(tracemind_engine PRIVATE tracemind_model)

add_executable(circuit_parse_test tests/circuit_parse_test.cpp)
target_link_libraries(circuit_parse_test PRIVATE tracemind_model GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(circuit_parse_test)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake -S core -B core/build && cmake --build core/build && ctest --test-dir core/build --output-on-failure`
Expected: PASS with `CircuitParseTest.ParsesBasicCircuitJson`

- [ ] **Step 5: Scaffold backend, frontend, and sample circuit**

```python
# backend/app/main.py
from fastapi import FastAPI

app = FastAPI(title="TraceMind API")


@app.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok"}
```

```text
# backend/requirements.txt
fastapi==0.116.1
uvicorn==0.35.0
pytest==8.4.1
httpx==0.28.1
```

```json
// frontend/package.json
{
  "name": "tracemind-frontend",
  "private": true,
  "version": "0.0.1",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "tsc && vite build",
    "test": "vitest run"
  },
  "dependencies": {
    "react": "^18.3.1",
    "react-dom": "^18.3.1"
  },
  "devDependencies": {
    "@types/react": "^18.3.12",
    "@types/react-dom": "^18.3.1",
    "@vitejs/plugin-react": "^4.4.1",
    "typescript": "^5.9.2",
    "vite": "^7.1.3",
    "vitest": "^3.2.4"
  }
}
```

```json
// data/sample_circuits/led_resistor.json
{
  "board": { "width": 24, "height": 14 },
  "components": [
    {
      "id": "BAT1",
      "kind": "battery",
      "width": 3,
      "height": 2,
      "pins": [
        { "id": "POS", "x": 2, "y": 1 },
        { "id": "NEG", "x": 0, "y": 1 }
      ]
    },
    {
      "id": "R1",
      "kind": "resistor",
      "width": 3,
      "height": 1,
      "pins": [
        { "id": "A", "x": 0, "y": 0 },
        { "id": "B", "x": 2, "y": 0 }
      ]
    },
    {
      "id": "LED1",
      "kind": "led",
      "width": 2,
      "height": 1,
      "pins": [
        { "id": "ANODE", "x": 0, "y": 0 },
        { "id": "CATHODE", "x": 1, "y": 0 }
      ]
    }
  ],
  "nets": [
    { "id": "VCC", "connections": [{ "component": "BAT1", "pin": "POS" }, { "component": "R1", "pin": "A" }] },
    { "id": "SIG", "connections": [{ "component": "R1", "pin": "B" }, { "component": "LED1", "pin": "ANODE" }] },
    { "id": "GND", "connections": [{ "component": "LED1", "pin": "CATHODE" }, { "component": "BAT1", "pin": "NEG" }] }
  ]
}
```

- [ ] **Step 6: Run basic backend and frontend verification**

Run: `python -m pytest backend`
Expected: PASS or `collected 0 items`

Run: `npm install --prefix frontend && npm run build --prefix frontend`
Expected: successful Vite production build

- [ ] **Step 7: Commit**

```bash
git add core backend frontend data tasks/todo.md
git commit -m "feat: scaffold TraceMind MVP workspace"
```

### Task 2: Implement tested placement events in the C++ engine

**Files:**
- Create: `core/src/placer/placer.hpp`
- Create: `core/src/placer/placer.cpp`
- Create: `core/src/io/run_result.hpp`
- Create: `core/src/io/run_result.cpp`
- Create: `core/tests/placer_test.cpp`
- Modify: `core/src/main.cpp`
- Modify: `core/CMakeLists.txt`

- [ ] **Step 1: Write the failing placement test**

```cpp
#include "model/circuit.hpp"
#include "placer/placer.hpp"

#include <gtest/gtest.h>

TEST(PlacerTest, ProducesNonOverlappingPlacementsWithinBoardBounds) {
  const auto circuit = tracemind::model::ParseCircuit(R"json(
  {
    "board": { "width": 18, "height": 10 },
    "components": [
      { "id": "U1", "kind": "chip", "width": 3, "height": 2, "pins": [{ "id": "P1", "x": 1, "y": 0 }] },
      { "id": "R1", "kind": "resistor", "width": 3, "height": 1, "pins": [{ "id": "A", "x": 0, "y": 0 }] }
    ],
    "nets": []
  }
  )json");

  const auto result = tracemind::placer::PlaceComponents(circuit);

  ASSERT_EQ(result.final_positions.size(), 2u);
  for (const auto& placement : result.final_positions) {
    EXPECT_GE(placement.x, 0);
    EXPECT_GE(placement.y, 0);
    EXPECT_LT(placement.x + placement.width, circuit.board_width + 1);
    EXPECT_LT(placement.y + placement.height, circuit.board_height + 1);
  }

  EXPECT_FALSE(result.steps.empty());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build core/build && ctest --test-dir core/build --output-on-failure`
Expected: FAIL because the placer does not exist yet.

- [ ] **Step 3: Write the minimal placement implementation**

```cpp
// core/src/placer/placer.hpp
#pragma once

#include "model/circuit.hpp"

#include <string>
#include <vector>

namespace tracemind::placer {

struct Placement {
  std::string component_id;
  int x;
  int y;
  int width;
  int height;
};

struct PlacementStep {
  int iteration;
  std::vector<Placement> placements;
};

struct PlacementResult {
  std::vector<PlacementStep> steps;
  std::vector<Placement> final_positions;
};

PlacementResult PlaceComponents(const model::Circuit& circuit);

}  // namespace tracemind::placer
```

```cpp
// core/src/placer/placer.cpp
#include "placer/placer.hpp"

namespace tracemind::placer {

PlacementResult PlaceComponents(const model::Circuit& circuit) {
  PlacementResult result;
  int cursor_x = 1;
  int cursor_y = 1;
  int row_height = 0;

  for (const auto& component : circuit.components) {
    if (cursor_x + component.width > circuit.board_width) {
      cursor_x = 1;
      cursor_y += row_height + 1;
      row_height = 0;
    }

    result.final_positions.push_back(Placement{
        .component_id = component.id,
        .x = cursor_x,
        .y = cursor_y,
        .width = component.width,
        .height = component.height,
    });

    result.steps.push_back(PlacementStep{
        .iteration = static_cast<int>(result.steps.size()),
        .placements = result.final_positions,
    });

    cursor_x += component.width + 1;
    if (component.height > row_height) {
      row_height = component.height;
    }
  }

  return result;
}

}  // namespace tracemind::placer
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build core/build && ctest --test-dir core/build --output-on-failure`
Expected: PASS with `PlacerTest.ProducesNonOverlappingPlacementsWithinBoardBounds`

- [ ] **Step 5: Commit**

```bash
git add core
git commit -m "feat: add initial component placement engine"
```

### Task 3: Expose engine runs through FastAPI

**Files:**
- Create: `backend/tests/test_health.py`
- Create: `backend/tests/test_circuits.py`
- Create: `backend/tests/test_runs.py`
- Create: `backend/app/schemas/circuit.py`
- Create: `backend/app/services/circuit_loader.py`
- Create: `backend/app/services/engine_runner.py`
- Modify: `backend/app/main.py`

- [ ] **Step 1: Write the failing backend tests**

```python
from fastapi.testclient import TestClient

from app.main import app

client = TestClient(app)


def test_health_returns_ok() -> None:
    response = client.get("/health")

    assert response.status_code == 200
    assert response.json() == {"status": "ok"}


def test_lists_sample_circuits() -> None:
    response = client.get("/api/circuits")

    assert response.status_code == 200
    body = response.json()
    assert len(body["circuits"]) >= 1
    assert body["circuits"][0]["id"] == "led_resistor"
```

```python
from fastapi.testclient import TestClient

from app.main import app

client = TestClient(app)


def test_creates_run_for_sample_circuit() -> None:
    response = client.post("/api/runs", json={"circuitId": "led_resistor"})

    assert response.status_code == 200
    body = response.json()
    assert body["runId"]
    assert body["events"][0]["type"] == "run_started"
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `python -m pytest backend/tests -v`
Expected: FAIL because sample circuit and run endpoints do not exist yet.

- [ ] **Step 3: Write the minimal backend implementation**

```python
# backend/app/services/circuit_loader.py
from __future__ import annotations

import json
from pathlib import Path


DATA_DIR = Path(__file__).resolve().parents[3] / "data" / "sample_circuits"


def list_circuits() -> list[dict[str, str]]:
    circuits: list[dict[str, str]] = []
    for path in sorted(DATA_DIR.glob("*.json")):
      circuits.append({"id": path.stem, "name": path.stem.replace("_", " ").title()})
    return circuits


def load_circuit(circuit_id: str) -> dict:
    path = DATA_DIR / f"{circuit_id}.json"
    return json.loads(path.read_text())
```

```python
# backend/app/services/engine_runner.py
from __future__ import annotations

import json
import uuid
from pathlib import Path


def run_circuit(circuit_id: str, circuit: dict) -> dict:
    run_id = str(uuid.uuid4())
    components = circuit.get("components", [])
    events = [
        {"runId": run_id, "stage": "run", "type": "run_started", "payload": {"circuitId": circuit_id}},
        {"runId": run_id, "stage": "placement", "type": "placement_complete", "payload": {"componentCount": len(components)}},
        {"runId": run_id, "stage": "routing", "type": "routing_complete", "payload": {"netCount": len(circuit.get("nets", []))}},
        {"runId": run_id, "stage": "run", "type": "run_complete", "payload": {"status": "ok"}},
    ]
    return {"runId": run_id, "events": events}
```

```python
# backend/app/main.py
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel

from app.services.circuit_loader import list_circuits, load_circuit
from app.services.engine_runner import run_circuit

app = FastAPI(title="TraceMind API")


class RunRequest(BaseModel):
    circuitId: str


@app.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok"}


@app.get("/api/circuits")
def circuits() -> dict[str, list[dict[str, str]]]:
    return {"circuits": list_circuits()}


@app.post("/api/runs")
def create_run(request: RunRequest) -> dict:
    try:
        circuit = load_circuit(request.circuitId)
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail="Circuit not found") from exc

    return run_circuit(request.circuitId, circuit)
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `python -m pytest backend/tests -v`
Expected: PASS for health, circuit listing, and run creation

- [ ] **Step 5: Commit**

```bash
git add backend
git commit -m "feat: add backend circuit and run APIs"
```

### Task 4: Render the initial dashboard in React

**Files:**
- Create: `frontend/src/lib/types.ts`
- Create: `frontend/src/components/CircuitList.tsx`
- Create: `frontend/src/components/MetricsPanel.tsx`
- Create: `frontend/src/components/BoardView.tsx`
- Create: `frontend/src/components/CommentaryPanel.tsx`
- Create: `frontend/src/components/__tests__/App.test.tsx`
- Modify: `frontend/package.json`
- Modify: `frontend/src/App.tsx`
- Modify: `frontend/src/styles.css`

- [ ] **Step 1: Write the failing frontend test**

```tsx
import { render, screen } from "@testing-library/react";
import App from "../App";

test("renders TraceMind dashboard shell", () => {
  render(<App />);

  expect(screen.getByText("TraceMind")).toBeInTheDocument();
  expect(screen.getByText("Sample Circuits")).toBeInTheDocument();
  expect(screen.getByText("Metrics")).toBeInTheDocument();
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `npm test --prefix frontend`
Expected: FAIL because the dashboard components and test tooling are not configured yet.

- [ ] **Step 3: Write the minimal frontend implementation**

```tsx
// frontend/src/App.tsx
import { BoardView } from "./components/BoardView";
import { CircuitList } from "./components/CircuitList";
import { CommentaryPanel } from "./components/CommentaryPanel";
import { MetricsPanel } from "./components/MetricsPanel";
import "./styles.css";

export default function App() {
  return (
    <main className="app-shell">
      <header className="hero">
        <p className="eyebrow">Mini PCB Design Automation Engine</p>
        <h1>TraceMind</h1>
      </header>
      <section className="layout">
        <CircuitList />
        <BoardView />
        <MetricsPanel />
        <CommentaryPanel />
      </section>
    </main>
  );
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `npm test --prefix frontend`
Expected: PASS for `renders TraceMind dashboard shell`

- [ ] **Step 5: Run production build**

Run: `npm run build --prefix frontend`
Expected: successful Vite production build

- [ ] **Step 6: Commit**

```bash
git add frontend
git commit -m "feat: add TraceMind dashboard shell"
```
