import "@testing-library/jest-dom/vitest";
import { render, screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, expect, test, vi } from "vitest";

import App from "../../App";

afterEach(() => {
  vi.unstubAllGlobals();
});

test("loads circuits and runs the selected sample", async () => {
  const fetchMock = vi.fn(async (input: RequestInfo | URL, init?: RequestInit) => {
    const url = String(input);

    if (url.includes("/api/circuits")) {
      return new Response(
        JSON.stringify({
          circuits: [{ id: "led_resistor", name: "Led Resistor" }],
        }),
        { status: 200 },
      );
    }

    if (url.includes("/api/runs")) {
      return new Response(
        JSON.stringify({
          runId: "run-1",
          circuitId: "led_resistor",
          board: { width: 24, height: 14 },
          components: [
            {
              id: "BAT1",
              kind: "battery",
              width: 3,
              height: 2,
              x: 1,
              y: 1,
              pins: [],
            },
            {
              id: "R1",
              kind: "resistor",
              width: 3,
              height: 1,
              x: 5,
              y: 1,
              pins: [],
            },
            {
              id: "LED1",
              kind: "led",
              width: 2,
              height: 1,
              x: 9,
              y: 1,
              pins: [],
            },
          ],
          traces: [
            { netId: "VCC", path: [{ x: 3, y: 2 }, { x: 4, y: 2 }] },
            { netId: "SIG", path: [{ x: 7, y: 1 }, { x: 8, y: 1 }] },
            { netId: "GND", path: [{ x: 10, y: 1 }, { x: 11, y: 1 }] },
          ],
          metrics: {
            componentCount: 3,
            netCount: 3,
            routedNetCount: 3,
            unroutedNetCount: 0,
            totalTraceLength: 6,
            boardUtilizationPercent: 8.4,
            estimatedIrDropMv: 14.2,
            estimatedMaxTemperatureC: 29.3,
            drcViolationCount: 0,
          },
          events: [
            {
              stage: "run",
              type: "run_started",
              payload: { componentCount: 3, netCount: 3 },
            },
            {
              stage: "placement",
              type: "placement_step",
              payload: {
                iteration: 0,
                placements: [
                  { componentId: "BAT1", x: 1, y: 1, width: 3, height: 2 },
                ],
              },
            },
            {
              stage: "routing",
              type: "routing_step",
              payload: {
                routedCount: 3,
                traces: [
                  { netId: "VCC", path: [{ x: 3, y: 2 }, { x: 4, y: 2 }] },
                  { netId: "SIG", path: [{ x: 7, y: 1 }, { x: 8, y: 1 }] },
                  { netId: "GND", path: [{ x: 10, y: 1 }, { x: 11, y: 1 }] },
                ],
              },
            },
            {
              stage: "analysis",
              type: "analysis_complete",
              payload: { routedNetCount: 3 },
            },
          ],
          commentary: [
            { stage: "run", text: "Loaded 3 components.", source: "local" },
            {
              stage: "routing",
              text: "Routed 3 nets with 0 unrouted connections remaining.",
              source: "local",
            },
          ],
        }),
        { status: 200 },
      );
    }

    throw new Error(`Unexpected fetch call: ${url} ${init?.method ?? "GET"}`);
  });

  vi.stubGlobal("fetch", fetchMock);
  render(<App />);

  expect(screen.getByText("TraceMind")).toBeInTheDocument();

  expect(await screen.findByText("Led Resistor")).toBeInTheDocument();

  await userEvent.click(screen.getByRole("button", { name: /run selected circuit/i }));

  await waitFor(() => {
    expect(screen.getByText("3 / 3")).toBeInTheDocument();
  });
  expect(screen.getByText("Loaded 3 components.")).toBeInTheDocument();
});
