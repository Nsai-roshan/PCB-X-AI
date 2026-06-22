import { useEffect, useState } from "react";

import { BoardView }        from "./components/BoardView";
import { CircuitList }      from "./components/CircuitList";
import { CommentaryPanel }  from "./components/CommentaryPanel";
import { MetricsPanel }     from "./components/MetricsPanel";
import { ParametersPanel }  from "./components/ParametersPanel";
import type {
  CircuitSummary, CommentaryEntry, ComponentPlacement,
  ComponentView, OverlayMode, RunEvent, RunParams, RunResponse, TraceView,
} from "./lib/types";
import { DEFAULT_PARAMS } from "./lib/types";

type FrameState = {
  components: ComponentView[];
  traces: TraceView[];
  activeStage: RunEvent["stage"];
};

const STAGE_ORDER: Record<RunEvent["stage"], number> = {
  run: 0, placement: 1, routing: 2, analysis: 3,
};

function mergePlacements(components: ComponentView[], placements: ComponentPlacement[]): ComponentView[] {
  const m = new Map(placements.map((p) => [p.componentId, p]));
  return components.map((c) => { const p = m.get(c.id); return p ? { ...c, x: p.x, y: p.y } : c; });
}

function deriveFrame(run: RunResponse | null, eventIndex: number): FrameState {
  if (!run) return { components: [], traces: [], activeStage: "run" };
  let components = run.components.map((c) => ({ ...c, x: -c.width - 1, y: -c.height - 1 }));
  let traces: TraceView[] = [];
  let activeStage: RunEvent["stage"] = "run";
  for (let i = 0; i <= eventIndex && i < run.events.length; i++) {
    const event = run.events[i];
    activeStage = event.stage;
    if (event.type === "placement_step") {
      const pls = event.payload.placements as ComponentPlacement[] | undefined;
      if (pls) components = mergePlacements(run.components, pls);
    }
    if (event.type === "routing_step") {
      const rt = event.payload.traces as TraceView[] | undefined;
      if (rt) traces = rt;
    }
  }
  if (eventIndex >= run.events.length - 1) { components = run.components; traces = run.traces; }
  return { components, traces, activeStage };
}

function visibleCommentary(commentary: CommentaryEntry[], activeStage: RunEvent["stage"]): CommentaryEntry[] {
  const maxStage = STAGE_ORDER[activeStage];
  return commentary.filter((item) => STAGE_ORDER[item.stage] <= maxStage);
}

export default function App() {
  const [circuits, setCircuits]             = useState<CircuitSummary[]>([]);
  const [selectedCircuitId, setSelectedId]  = useState<string>("");
  const [runResult, setRunResult]           = useState<RunResponse | null>(null);
  const [eventIndex, setEventIndex]         = useState(0);
  const [isLoadingCircuits, setIsLoading]   = useState(true);
  const [isRunning, setIsRunning]           = useState(false);
  const [isPlaying, setIsPlaying]           = useState(false);
  const [errorMessage, setErrorMessage]     = useState("");
  const [overlayMode, setOverlayMode]       = useState<OverlayMode>("none");
  const [explanation, setExplanation]       = useState<string>("");
  const [isExplaining, setIsExplaining]     = useState(false);
  const [params, setParams]                 = useState<RunParams>({ ...DEFAULT_PARAMS });

  useEffect(() => {
    let active = true;
    async function load() {
      setIsLoading(true);
      try {
        const res  = await fetch("/api/circuits");
        if (!res.ok) throw new Error("Failed to load circuits.");
        const data = (await res.json()) as { circuits: CircuitSummary[] };
        if (!active) return;
        setCircuits(data.circuits);
        setSelectedId((cur) => cur || data.circuits[0]?.id || "");
      } catch (e) {
        if (!active) return;
        setErrorMessage(e instanceof Error ? e.message : "Failed to load circuits.");
      } finally {
        if (active) setIsLoading(false);
      }
    }
    void load();
    return () => { active = false; };
  }, []);

  useEffect(() => {
    if (!isPlaying || !runResult) return;
    const timer = window.setInterval(() => {
      setEventIndex((cur) => {
        const next = cur + 1;
        if (next >= runResult.events.length) { window.clearInterval(timer); setIsPlaying(false); return runResult.events.length - 1; }
        return next;
      });
    }, 120);
    return () => window.clearInterval(timer);
  }, [isPlaying, runResult]);

  const frame       = deriveFrame(runResult, eventIndex);
  const commentary  = visibleCommentary(runResult?.commentary ?? [], frame.activeStage);
  const totalEvents = runResult?.events.length ?? 0;

  async function handleRun() {
    if (!selectedCircuitId) return;
    setIsRunning(true);
    setErrorMessage("");
    setExplanation("");
    setOverlayMode("none");
    setIsPlaying(false);
    try {
      const res = await fetch("/api/runs", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ circuitId: selectedCircuitId, params }),
      });
      if (!res.ok) {
        const err = (await res.json().catch(() => ({}))) as { detail?: string };
        throw new Error(err.detail || "Run failed.");
      }
      const data = (await res.json()) as RunResponse;
      setRunResult(data);
      setEventIndex(data.events.length - 1);
    } catch (e) {
      setErrorMessage(e instanceof Error ? e.message : "Run failed.");
    } finally {
      setIsRunning(false);
    }
  }

  async function handleExplain() {
    if (!selectedCircuitId) return;
    setIsExplaining(true);
    try {
      const res = await fetch(`/api/circuits/${selectedCircuitId}/explain`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: runResult ? JSON.stringify({ metrics: runResult.metrics }) : "{}",
      });
      if (!res.ok) throw new Error("Explain failed.");
      const data = (await res.json()) as { explanation: string };
      setExplanation(data.explanation);
    } catch {
      setExplanation("Could not load explanation. Check GROQ_API_KEY.");
    } finally {
      setIsExplaining(false);
    }
  }

  function handleSlider(e: React.ChangeEvent<HTMLInputElement>) {
    setIsPlaying(false);
    setEventIndex(Number(e.target.value));
  }

  function handlePlayPause() {
    if (!runResult) return;
    if (isPlaying) { setIsPlaying(false); return; }
    if (eventIndex >= totalEvents - 1) setEventIndex(0);
    setIsPlaying(true);
  }

  const fixSuggestions = runResult?.fixSuggestions ?? [];
  const metrics        = runResult?.metrics ?? null;

  return (
    <main className="app-shell">
      <header className="hero">
        <div>
          <p className="eyebrow">Mini PCB Design Automation Engine</p>
          <h1>TraceMind</h1>
        </div>
        <p className="lede">My personal project to understand PCB design — integrating a real IR-drop solver, Leiden+SA cell placer, and FDM thermal modeler to see how they interact and impact placement.</p>
      </header>

      <section className="dashboard-grid">
        {/* Column 1 â€” circuit picker + params */}
        <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
          <CircuitList
            circuits={circuits}
            selectedCircuitId={selectedCircuitId}
            onSelect={(id) => { setSelectedId(id); setRunResult(null); setExplanation(""); setOverlayMode("none"); }}
            onRun={handleRun}
            isLoading={isLoadingCircuits}
            isRunning={isRunning}
          />
          <ParametersPanel params={params} onChange={setParams} />
        </div>

        {/* Column 2 â€” board canvas + scrubber */}
        <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
          <div style={{ display: "flex", gap: "8px", padding: "4px 0" }}>
            {(["none", "irdrop", "thermal"] as OverlayMode[]).map((mode) => (
              <button key={mode} onClick={() => setOverlayMode(mode)} style={{
                padding: "4px 12px", borderRadius: "6px", border: "1px solid #555",
                background: overlayMode === mode ? "#f4b860" : "#1e2e28",
                color: overlayMode === mode ? "#15241f" : "#ccc",
                cursor: "pointer", fontSize: "13px",
                fontWeight: overlayMode === mode ? 700 : 400,
              }}>
                {mode === "none" ? "No overlay" : mode === "irdrop" ? "IR Drop" : "Thermal"}
              </button>
            ))}
          </div>

          <BoardView
            board={runResult?.board ?? null}
            components={frame.components}
            traces={frame.traces}
            eventIndex={eventIndex}
            totalEvents={totalEvents}
            activeStage={frame.activeStage}
            overlayMode={overlayMode}
            irDropGrid={runResult?.irDropGrid ?? []}
            thermalGrid={runResult?.thermalGrid ?? []}
          />

          {runResult && (
            <div style={{ display: "flex", alignItems: "center", gap: "10px", padding: "4px 0" }}>
              <button onClick={handlePlayPause} style={{
                padding: "4px 14px", borderRadius: "6px",
                background: "#1e2e28", border: "1px solid #555",
                color: "#ccc", cursor: "pointer", fontSize: "13px", whiteSpace: "nowrap",
              }}>
                {isPlaying ? "Pause" : eventIndex >= totalEvents - 1 ? "Replay" : "Play"}
              </button>
              <input
                type="range" min={0} max={Math.max(0, totalEvents - 1)} value={eventIndex}
                onChange={handleSlider}
                style={{ flex: 1, accentColor: "#f4b860" }}
              />
              <span style={{ fontSize: "12px", color: "#888", whiteSpace: "nowrap" }}>
                {frame.activeStage} {eventIndex + 1}/{totalEvents}
              </span>
            </div>
          )}
        </div>

        {/* Column 3 â€” metrics */}
        <MetricsPanel metrics={metrics} isPlaying={isPlaying} onTogglePlayback={handlePlayPause} canToggle={Boolean(runResult)} />

        {/* Column 4 â€” commentary + AI analysis */}
        <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
          <CommentaryPanel commentary={commentary} errorMessage={errorMessage} />
          <div className="panel" style={{ padding: "16px" }}>
            <p className="panel-kicker">AI Analysis</p>
            <button
              onClick={handleExplain}
              disabled={!selectedCircuitId || isExplaining}
              style={{
                marginBottom: "10px", padding: "6px 16px", borderRadius: "6px",
                background: "#76c7b7", border: "none", color: "#15241f",
                fontWeight: 700, cursor: "pointer",
              }}
            >
              {isExplaining ? "Analysingâ€¦" : runResult ? "Analyse results" : "Explain circuit"}
            </button>
            {explanation && (
              <p style={{ fontSize: "13px", lineHeight: 1.6, whiteSpace: "pre-wrap" }}>{explanation}</p>
            )}
            {fixSuggestions.length > 0 && (
              <>
                <p style={{ fontWeight: 700, marginTop: "12px", color: "#f0876a" }}>Fix suggestions:</p>
                <ul style={{ fontSize: "13px", lineHeight: 1.6, paddingLeft: "16px" }}>
                  {fixSuggestions.map((s, i) => <li key={i}>{s}</li>)}
                </ul>
              </>
            )}
          </div>
        </div>
      </section>
    </main>
  );
}
