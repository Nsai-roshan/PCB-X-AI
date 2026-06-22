# TraceMind Tier 1 MVP Design

## Summary

TraceMind is a small PCB design automation demo that takes a hand-authored circuit description, auto-places components, auto-routes traces, and visualizes the process live in a browser while an AI layer explains what is happening. The first milestone focuses on the end-to-end automation loop rather than the full analysis stack so we can get a compelling, demoable system working quickly.

This MVP intentionally implements the highest-priority slice from the project brief:

- component/netlist input
- C++ auto-placement
- C++ auto-routing
- FastAPI orchestration
- WebSocket event streaming
- React dashboard animation
- basic AI narration

IR-drop, DRC, and thermal are planned as follow-on milestones after the placement-routing pipeline is stable.

## Goals

- Build a working end-to-end demo that can load a tiny circuit and produce a routed board visualization.
- Keep the core automation in C++17 so the project aligns with the intended EDA and systems focus.
- Expose the engine through a simple backend contract so later analysis modules can be added without reshaping the UI.
- Make the demo understandable to non-PCB audiences through live narration, metrics, and animation.

## Non-Goals For Tier 1

- No real PCB CAD file parsing.
- No multi-layer routing in the first pass.
- No vias, copper pours, or power planes yet.
- No physics analysis in the first milestone runtime path.
- No production-grade PCB rules engine.

## Scope Decision

We considered three starting approaches:

1. Tier 1 MVP first: build the full placement-routing-demo loop before advanced analysis.
2. Full scaffold first: define every module and interface up front, then fill them in incrementally.
3. Engine-first: focus on the C++ core and API before building the richer UI experience.

We are choosing approach 1 because it produces the fastest credible demo while keeping the architecture extensible. It also maps directly to the project brief's priority guidance, where placement, routing, animation, and narration are the must-have story.

## User Experience

The user opens the dashboard, selects a sample circuit, and starts an automation run. The frontend streams events as the backend orchestrates the engine:

1. The circuit is loaded and validated.
2. The placer emits move-by-move component positions.
3. The router emits trace-by-trace routing results.
4. The dashboard animates each stage and updates metrics.
5. The AI panel narrates high-level reasoning in plain English.

The final view shows the placed and routed board with run metrics and a replayable event timeline.

## Architecture

### System Shape

- `core/`: C++17 engine for the board model, placement, routing, and event-log generation
- `backend/`: FastAPI service that validates inputs, launches the engine, streams events, and calls the AI narrator
- `frontend/`: React + TypeScript + Vite dashboard that renders the board, controls playback, and displays commentary
- `data/sample_circuits/`: authored sample circuit definitions used for demos and tests

### Data Boundaries

The engine owns board-state computation and emits structured JSON snapshots. The backend owns orchestration and AI commentary. The frontend owns rendering, animation control, and human-facing state. This keeps algorithm logic out of the UI and keeps AI out of the core engine.

### Execution Model

The first implementation uses a CLI-style C++ engine executable invoked by the backend. That choice minimizes integration complexity in an empty repo and keeps the engine independently testable. If needed later, the invocation boundary can evolve into a library or long-lived service without changing the frontend contract.

## Core Components

### Circuit Model

The project will define its own small JSON schema instead of using a real PCB format. A circuit file contains:

- board width and height in grid units
- components with id, type, footprint width/height, pins, and optional power metadata
- nets listing named connections between component pins

This keeps the input model simple and fully under our control.

### Auto-Placer

The placer will use a small simulated annealing loop over a 2D board grid. Its cost function will combine:

- estimated net wirelength
- overlap penalty
- out-of-bounds penalty

Each accepted move produces an event snapshot with component positions and current score. The goal is not industrial placement quality in Tier 1; the goal is visible optimization progress and a reasonable final layout.

### Auto-Router

The router will rasterize the placed board into a grid and route one net at a time using a maze-routing algorithm. The first pass will:

- treat component footprints as obstacles
- avoid already-routed traces
- route two-pin net segments on a single layer
- emit one event per completed segment

This keeps the routing problem bounded while still showing real obstacle-avoiding behavior.

### Backend Orchestrator

The backend will provide:

- REST endpoint to list sample circuits
- REST endpoint to start a run
- WebSocket endpoint to stream automation events
- small AI narration module that turns selected events into natural-language commentary

The backend is also where future IR-drop, thermal, and DRC modules will be composed into the pipeline.

### Frontend Dashboard

The frontend will provide:

- sample circuit picker
- board canvas for components and traces
- playback controls
- metrics panel
- AI commentary panel

The visual priority is clarity over polish: users should immediately understand placement settling, routing progress, and final board state.

## Data Flow

1. Frontend requests available sample circuits.
2. User starts a run for a chosen circuit.
3. Backend validates the circuit and launches the C++ engine.
4. Engine runs placement and emits placement events.
5. Engine runs routing and emits routing events.
6. Backend forwards events over WebSocket and optionally generates commentary from milestone events.
7. Frontend stores the event log, animates updates, and renders the final board state.

## API And Event Design

### Sample Circuit Endpoint

- `GET /api/circuits`
- returns available sample circuit metadata

### Run Endpoint

- `POST /api/runs`
- request body contains a circuit id or inline circuit payload
- response returns a run id

### Event Stream

- `GET /ws/runs/{run_id}`
- event types include `run_started`, `placement_step`, `placement_complete`, `routing_step`, `routing_complete`, `commentary`, `run_complete`, and `run_error`

Every event includes:

- `runId`
- `timestamp`
- `stage`
- `type`
- `payload`

This event schema is the key contract between engine, backend, and frontend.

## Error Handling

- Invalid circuit definitions fail fast in the backend with actionable messages.
- Engine failures surface as `run_error` events and a backend log entry.
- Unroutable nets do not crash the run; they produce routing failure details and the run completes with warnings.
- Missing AI configuration disables commentary gracefully instead of blocking the run.

## Testing Strategy

### Core Tests

- circuit JSON parsing and validation
- geometry helpers for overlap and bounds checking
- placement smoke tests to verify cost decreases or valid placements are produced
- routing smoke tests to verify simple nets can be routed around obstacles

### Backend Tests

- sample circuit listing
- run creation
- event stream lifecycle
- AI narration fallback when no provider key is configured

### Frontend Tests

- dashboard loads sample circuits
- streamed placement and routing events update rendered state
- playback controls and metrics remain in sync with the event log

### End-to-End Verification

One scripted happy-path demo circuit should prove the whole flow works from sample selection through final routed board.

## Future Module Plan

### IR-Drop Integration

The later IR-drop milestone should reuse concepts and possibly code boundaries from the existing `Nsai-roshan/Power-Grid-IR-Drop-Analyzer` repository, which models a 2D resistive mesh, assembles a sparse conductance matrix, solves `Gx = i`, and exports CSV results for visualization. In TraceMind, that solver will be retargeted from synthetic chip power grids to PCB power geometry after routing exists. The MVP architecture keeps this future module behind the backend pipeline so the integration can be added without reworking the UI foundation.

### DRC And Thermal

After routing is stable, DRC can operate on the routed geometry and thermal can operate on a board raster plus per-component power values. Both are downstream analyses and do not need to block the first milestone.

## Initial Repo Structure

```text
tracemind/
  core/
    CMakeLists.txt
    src/
      model/
      placer/
      router/
      io/
  backend/
    app/
      api/
      services/
      schemas/
  frontend/
    src/
      components/
      hooks/
      lib/
      pages/
  data/
    sample_circuits/
  docs/
    superpowers/
      specs/
      plans/
```

## Acceptance Criteria For Tier 1

- A sample circuit can be loaded from the project data directory.
- The backend can launch the C++ engine and receive structured events.
- The placer produces visible stepwise progress and a legal final placement.
- The router produces visible routed segments for at least simple sample circuits.
- The frontend animates the event stream and shows final board state.
- AI narration appears during the run when configured and degrades cleanly when not configured.
- The demo can be run locally from a clean checkout using documented setup steps.

## Risks And Mitigations

- Routing complexity may balloon if the circuit model is too ambitious.
  Mitigation: keep sample circuits tiny and mostly two-pin or low-fanout nets at first.
- Cross-language integration may slow iteration.
  Mitigation: keep the engine contract file- or stdout-driven in the first pass.
- AI latency could disrupt the live feel.
  Mitigation: narrate milestone events rather than every microscopic engine step.

## Recommendation

Proceed with the Tier 1 MVP exactly as scoped here, then add IR-drop as the first post-MVP analysis module by reusing the existing analyzer's sparse-solver architecture once routed power geometry exists.
