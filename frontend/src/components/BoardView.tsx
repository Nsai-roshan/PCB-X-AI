import type { ComponentView, OverlayMode, RunEvent, TraceView } from "../lib/types";

type BoardViewProps = {
  board: { width: number; height: number } | null;
  components: ComponentView[];
  traces: TraceView[];
  eventIndex: number;
  totalEvents: number;
  activeStage: RunEvent["stage"];
  overlayMode: OverlayMode;
  irDropGrid: number[];
  thermalGrid: number[];
};

const TRACE_COLORS = ["#f4b860", "#76c7b7", "#f0876a", "#95a4ff"];

// Map a normalised value 0-1 to a green→yellow→red colour.
function heatColor(t: number): string {
  const clamped = Math.max(0, Math.min(1, t));
  if (clamped < 0.5) {
    // green → yellow
    const r = Math.round(clamped * 2 * 255);
    return `rgba(${r},200,50,0.55)`;
  }
  // yellow → red
  const g = Math.round((1 - (clamped - 0.5) * 2) * 200);
  return `rgba(255,${g},30,0.60)`;
}

function HeatOverlay({
  grid,
  boardWidth,
  boardHeight,
  cell,
}: {
  grid: number[];
  boardWidth: number;
  boardHeight: number;
  cell: number;
}) {
  if (grid.length === 0) return null;
  const max = Math.max(...grid, 1e-9);
  const cells: JSX.Element[] = [];
  for (let y = 0; y < boardHeight; y++) {
    for (let x = 0; x < boardWidth; x++) {
      const val = grid[y * boardWidth + x] ?? 0;
      if (val <= 0) continue;
      cells.push(
        <rect
          key={`${x}-${y}`}
          x={x * cell}
          y={y * cell}
          width={cell}
          height={cell}
          fill={heatColor(val / max)}
        />
      );
    }
  }
  return <>{cells}</>;
}

export function BoardView({
  board,
  components,
  traces,
  eventIndex,
  totalEvents,
  activeStage,
  overlayMode,
  irDropGrid,
  thermalGrid,
}: BoardViewProps) {
  const width = board?.width ?? 24;
  const height = board?.height ?? 14;
  const cell = 24;

  const overlayGrid =
    overlayMode === "irdrop"
      ? irDropGrid
      : overlayMode === "thermal"
      ? thermalGrid
      : [];

  return (
    <section className="panel board-panel">
      <div className="panel-header board-header">
        <div>
          <p className="panel-kicker">Board View</p>
          <h2>Automation Canvas</h2>
        </div>
        <div className="board-badge">
          <span>Stage</span>
          <strong>{activeStage}</strong>
        </div>
      </div>
      <div className="board-stage">
        <svg
          className="board-svg"
          viewBox={`0 0 ${width * cell} ${height * cell}`}
          role="img"
          aria-label="PCB routing board"
        >
          <defs>
            <pattern
              id="board-grid"
              width={cell}
              height={cell}
              patternUnits="userSpaceOnUse"
            >
              <path
                d={`M ${cell} 0 L 0 0 0 ${cell}`}
                fill="none"
                stroke="rgba(255,255,255,0.08)"
                strokeWidth="1"
              />
            </pattern>
          </defs>
          <rect width="100%" height="100%" fill="url(#board-grid)" />

          {/* Traces */}
          {traces.map((trace, index) => (
            <polyline
              key={trace.netId}
              points={trace.path
                .map((p) => `${p.x * cell + cell / 2},${p.y * cell + cell / 2}`)
                .join(" ")}
              fill="none"
              stroke={TRACE_COLORS[index % TRACE_COLORS.length]}
              strokeWidth={Math.max(5, cell * 0.24)}
              strokeLinecap="round"
              strokeLinejoin="round"
            />
          ))}

          {/* Heat overlay (IR drop or thermal) */}
          {overlayMode !== "none" && (
            <HeatOverlay
              grid={overlayGrid}
              boardWidth={width}
              boardHeight={height}
              cell={cell}
            />
          )}

          {/* Components */}
          {components.map((component) => (
            <g key={component.id}>
              <rect
                x={component.x * cell}
                y={component.y * cell}
                width={component.width * cell}
                height={component.height * cell}
                rx="10"
                fill="#f4e6c8"
                stroke="#15241f"
                strokeWidth="2"
              />
              <text
                x={component.x * cell + 10}
                y={component.y * cell + 24}
                fill="#15241f"
                fontSize="14"
                fontWeight="700"
              >
                {component.id}
              </text>
            </g>
          ))}
        </svg>
      </div>
      <p className="board-footer">
        Timeline step {totalEvents === 0 ? 0 : eventIndex + 1} / {totalEvents}
      </p>
    </section>
  );
}
