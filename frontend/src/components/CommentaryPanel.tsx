import type { CommentaryEntry } from "../lib/types";

type CommentaryPanelProps = {
  commentary: CommentaryEntry[];
  errorMessage: string;
};

export function CommentaryPanel({
  commentary,
  errorMessage,
}: CommentaryPanelProps) {
  return (
    <section className="panel">
      <div className="panel-header">
        <p className="panel-kicker">AI Layer</p>
        <h2>Commentary</h2>
      </div>
      {errorMessage ? <p className="panel-error">{errorMessage}</p> : null}
      {commentary.length === 0 ? (
        <p className="panel-empty">
          Run a circuit to generate placement, routing, and analysis commentary.
        </p>
      ) : (
        <ul className="commentary-list">
          {commentary.map((entry) => (
            <li key={`${entry.stage}-${entry.text}`}>
              <strong>{entry.stage}</strong>
              <span>{entry.text}</span>
              <small>{entry.source ?? "local"}</small>
            </li>
          ))}
        </ul>
      )}
    </section>
  );
}
