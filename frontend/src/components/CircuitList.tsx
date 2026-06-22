import type { CircuitSummary } from "../lib/types";

type CircuitListProps = {
  circuits: CircuitSummary[];
  selectedCircuitId: string;
  onSelect: (circuitId: string) => void;
  onRun: () => void;
  isLoading: boolean;
  isRunning: boolean;
};

export function CircuitList({
  circuits,
  selectedCircuitId,
  onSelect,
  onRun,
  isLoading,
  isRunning,
}: CircuitListProps) {
  return (
    <section className="panel">
      <div className="panel-header">
        <p className="panel-kicker">Inputs</p>
        <h2>Sample Circuits</h2>
      </div>
      {isLoading ? (
        <p className="panel-empty">Loading sample circuits...</p>
      ) : (
        <>
          <ul className="stack-list">
            {circuits.map((circuit) => (
              <li
                key={circuit.id}
                className={`list-card ${selectedCircuitId === circuit.id ? "selected-card" : ""}`}
              >
                <button
                  type="button"
                  className="circuit-select"
                  onClick={() => onSelect(circuit.id)}
                >
                  <span>{circuit.name}</span>
                  <small>{circuit.id}</small>
                </button>
              </li>
            ))}
          </ul>
          <button
            type="button"
            className="primary-action"
            onClick={onRun}
            disabled={!selectedCircuitId || isRunning}
          >
            {isRunning ? "Running..." : "Run Selected Circuit"}
          </button>
        </>
      )}
    </section>
  );
}
