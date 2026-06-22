export type CircuitSummary = { id: string; name: string };
export type GridPoint      = { x: number; y: number };
export type Pin            = { id: string; x: number; y: number };

export type ComponentPlacement = {
  componentId: string; x: number; y: number; width: number; height: number;
};

export type ComponentView = {
  id: string; kind: string; width: number; height: number; x: number; y: number;
  pins: Pin[];
};

export type TraceView = { netId: string; path: GridPoint[] };

export type RunParams = {
  traceResistancePerCell: number;
  loadCurrentPerPinA:     number;
  violationThresholdMv:   number;
  supplyVoltage:          number;
  mcuPowerW:              number;
  ledPowerW:              number;
  defaultPowerW:          number;
};

export const DEFAULT_PARAMS: RunParams = {
  traceResistancePerCell: 0.05,
  loadCurrentPerPinA:     0.01,
  violationThresholdMv:   30,
  supplyVoltage:          3.3,
  mcuPowerW:              0.15,
  ledPowerW:              0.05,
  defaultPowerW:          0.02,
};

export type RunMetrics = {
  componentCount: number;
  netCount: number;
  routedNetCount: number;
  unroutedNetCount: number;
  totalTraceLength: number;
  boardUtilizationPercent: number;
  maxIrDropMv: number;
  irViolationCount: number;
  irSolverConverged: boolean;
  maxTemperatureC: number;
  minTemperatureC: number;
  thermalIterations: number;
  drcViolationCount: number;
};

export type RunEvent = {
  runId?: string;
  stage: "run" | "placement" | "routing" | "analysis";
  type: string;
  payload: Record<string, unknown>;
};

export type CommentaryEntry = {
  stage: "run" | "placement" | "routing" | "analysis";
  text: string;
  source?: string;
};

export type OverlayMode = "none" | "irdrop" | "thermal";

export type RunResponse = {
  runId: string;
  circuitId: string;
  board: { width: number; height: number };
  components: ComponentView[];
  traces: TraceView[];
  metrics: RunMetrics;
  irDropGrid: number[];
  thermalGrid: number[];
  events: RunEvent[];
  commentary: CommentaryEntry[];
  fixSuggestions: string[];
};