import type { RunMetrics } from "../lib/types";

type MetricsPanelProps = {
  metrics: RunMetrics | null;
  isPlaying: boolean;
  onTogglePlayback: () => void;
  canToggle: boolean;
};

export function MetricsPanel({
  metrics,
  isPlaying,
  onTogglePlayback,
  canToggle,
}: MetricsPanelProps) {
  const cards = metrics
    ? [
        { label: "Components",  value: `${metrics.componentCount}` },
        { label: "Routed Nets", value: `${metrics.routedNetCount} / ${metrics.netCount}` },
        { label: "Trace Length", value: `${metrics.totalTraceLength}` },
        { label: "Utilization", value: `${metrics.boardUtilizationPercent.toFixed(1)}%` },
        { label: "IR Drop",     value: `${metrics.maxIrDropMv.toFixed(1)} mV` },
        { label: "IR Violations", value: `${metrics.irViolationCount}` },
        { label: "Max Temp",    value: `${metrics.maxTemperatureC.toFixed(1)} C` },
        { label: "DRC",         value: `${metrics.drcViolationCount}` },
      ]
    : [
        { label: "Components",  value: "--" },
        { label: "Routed Nets", value: "--" },
        { label: "Trace Length", value: "--" },
        { label: "Utilization", value: "--" },
        { label: "IR Drop",     value: "--" },
        { label: "Max Temp",    value: "--" },
      ];

  return (
    <section className="panel">
      <div className="panel-header">
        <p className="panel-kicker">Run Data</p>
        <h2>Metrics</h2>
      </div>
      <div className="metric-grid">
        {cards.map((metric) => (
          <article key={metric.label} className="metric-card">
            <span>{metric.label}</span>
            <strong>{metric.value}</strong>
          </article>
        ))}
      </div>
      <button
        type="button"
        className="secondary-action"
        onClick={onTogglePlayback}
        disabled={!canToggle}
      >
        {isPlaying ? "Pause Timeline" : "Play Timeline"}
      </button>
    </section>
  );
}
