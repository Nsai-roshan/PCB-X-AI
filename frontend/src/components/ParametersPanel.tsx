import { useState } from "react";
import type { RunParams } from "../lib/types";
import { DEFAULT_PARAMS } from "../lib/types";

// ── Tooltip ────────────────────────────────────────────────────────────────
function Tooltip({ text }: { text: string }) {
  const [visible, setVisible] = useState(false);
  return (
    <span
      style={{ position: "relative", display: "inline-flex", alignItems: "center", marginLeft: "5px" }}
      onMouseEnter={() => setVisible(true)}
      onMouseLeave={() => setVisible(false)}
    >
      <span style={{
        display: "inline-flex", alignItems: "center", justifyContent: "center",
        width: "14px", height: "14px", borderRadius: "50%",
        background: "#2e4a40", border: "1px solid #4a7a6a",
        color: "#76c7b7", fontSize: "9px", fontWeight: 700,
        cursor: "help", userSelect: "none", lineHeight: 1,
      }}>?</span>
      {visible && (
        <span style={{
          position: "absolute", left: "20px", top: "50%", transform: "translateY(-50%)",
          width: "220px", background: "#0f1f18", border: "1px solid #2e4a40",
          borderRadius: "8px", padding: "10px 12px", zIndex: 100,
          fontSize: "11px", color: "#ccc", lineHeight: 1.55,
          boxShadow: "0 4px 20px rgba(0,0,0,0.6)",
          pointerEvents: "none",
        }}>
          {text}
        </span>
      )}
    </span>
  );
}

// ── Slider definitions ──────────────────────────────────────────────────────
type SliderDef = {
  key: keyof RunParams;
  label: string;
  min: number; max: number; step: number; unit: string;
  tip: string;   // short hint under slider
  info: string;  // full tooltip on hover
};

const SLIDERS: SliderDef[] = [
  {
    key: "traceResistancePerCell",
    label: "Trace resistance",
    min: 0.001, max: 0.5, step: 0.001, unit: "Ω/cell",
    tip: "Lower = thicker/wider power traces → less IR drop",
    info: "How much resistance each grid cell of a power trace has.\n\n⬆ Higher → traces act like thin wire — voltage drops a lot by the time it reaches far components (high IR drop).\n\n⬇ Lower → traces act like thick copper bus — voltage stays nearly constant everywhere (low IR drop).\n\nDefault 0.05 Ω/cell. Real PCB equivalent: changing copper weight or trace width.",
  },
  {
    key: "loadCurrentPerPinA",
    label: "Current per load pin",
    min: 0.001, max: 0.1, step: 0.001, unit: "A",
    tip: "Higher = heavier component loads → more IR drop",
    info: "How much current each component pin draws from the power rail.\n\n⬆ Higher → components suck more power, voltage drops further along the trace (more IR drop).\n\n⬇ Lower → components draw less, voltage stays stable.\n\nDefault 0.01 A (10 mA). Real equivalent: the actual current consumption of your ICs.",
  },
  {
    key: "violationThresholdMv",
    label: "IR violation threshold",
    min: 5, max: 100, step: 1, unit: "mV",
    tip: "Cells above this are counted as violations",
    info: "The maximum acceptable voltage drop before a cell is flagged red.\n\n⬆ Higher → fewer violations reported (you are being lenient about how much drop is OK).\n\n⬇ Lower → more cells flagged (you are being strict — small drops count as problems).\n\nDefault 30 mV. Real PCBs typically allow 50–100 mV on power rails.",
  },
  {
    key: "supplyVoltage",
    label: "Supply voltage",
    min: 1.8, max: 5.0, step: 0.1, unit: "V",
    tip: "VCC rail voltage fed to the IR-drop solver",
    info: "The voltage your power supply provides to the board.\n\n⬆ Higher (e.g. 5 V) → the same absolute IR drop is a smaller percentage of supply, so components still work.\n\n⬇ Lower (e.g. 1.8 V) → even a small IR drop is a large percentage and may cause malfunctions.\n\nDefault 3.3 V (typical microcontroller supply).",
  },
  {
    key: "mcuPowerW",
    label: "MCU / IC power",
    min: 0.01, max: 0.5, step: 0.01, unit: "W",
    tip: "Power dissipated per MCU/IC/SoC → drives thermal hotspots",
    info: "How many watts each microcontroller or IC chip dissipates as heat.\n\n⬆ Higher → those chips run hotter; you will see a bright red hotspot on the thermal overlay.\n\n⬇ Lower → chips stay cool, thermal map stays mostly green.\n\nDefault 0.15 W. Real equivalent: the power dissipation spec on your chip datasheet.",
  },
  {
    key: "ledPowerW",
    label: "LED power",
    min: 0.005, max: 0.2, step: 0.005, unit: "W",
    tip: "Power dissipated per LED",
    info: "How many watts each LED dissipates as heat.\n\n⬆ Higher → LEDs generate more heat; visible as warm spots on the thermal overlay.\n\n⬇ Lower → LEDs barely affect temperature.\n\nDefault 0.05 W (typical indicator LED at ~20 mA).",
  },
  {
    key: "defaultPowerW",
    label: "Passive power",
    min: 0.001, max: 0.1, step: 0.001, unit: "W",
    tip: "Power per resistor / capacitor / other component",
    info: "How many watts each resistor, capacitor, or unrecognised component dissipates.\n\n⬆ Higher → even passive components contribute to heating; useful to model resistors carrying high current.\n\n⬇ Lower → passives are treated as cool (realistic for signal-path resistors).\n\nDefault 0.02 W.",
  },
];

// ── Component ───────────────────────────────────────────────────────────────
type Props = { params: RunParams; onChange: (p: RunParams) => void };

export function ParametersPanel({ params, onChange }: Props) {
  function set(key: keyof RunParams, value: number) {
    onChange({ ...params, [key]: value });
  }

  return (
    <div className="panel" style={{ padding: "16px" }}>
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "4px" }}>
        <p className="panel-kicker" style={{ margin: 0 }}>Solver Parameters</p>
        <button
          onClick={() => onChange({ ...DEFAULT_PARAMS })}
          style={{
            fontSize: "11px", padding: "2px 10px", borderRadius: "4px",
            background: "#1e2e28", border: "1px solid #555", color: "#aaa", cursor: "pointer",
          }}
        >Reset</button>
      </div>
      <p style={{ fontSize: "11px", color: "#888", marginBottom: "14px", lineHeight: 1.4 }}>
        Tweak and re-run to see how physics changes. Hover <strong>?</strong> for an explanation.
      </p>

      {SLIDERS.map(({ key, label, min, max, step, unit, tip, info }) => {
        const val     = params[key];
        const changed = val !== DEFAULT_PARAMS[key];
        return (
          <div key={key} style={{ marginBottom: "16px" }}>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "3px" }}>
              <span style={{ display: "flex", alignItems: "center" }}>
                <span style={{ fontSize: "12px", fontWeight: 600, color: changed ? "#f4b860" : "#ccc" }}>
                  {label}
                </span>
                <Tooltip text={info} />
              </span>
              <span style={{ fontSize: "12px", color: changed ? "#f4b860" : "#888", fontVariantNumeric: "tabular-nums" }}>
                {val.toFixed(step < 0.01 ? 3 : step < 1 ? 2 : 0)} {unit}
              </span>
            </div>
            <input
              type="range" min={min} max={max} step={step} value={val}
              onChange={(e) => set(key, parseFloat(e.target.value))}
              style={{ width: "100%", accentColor: changed ? "#f4b860" : "#76c7b7" }}
            />
            <p style={{ fontSize: "10px", color: "#666", margin: "2px 0 0", lineHeight: 1.3 }}>{tip}</p>
          </div>
        );
      })}
    </div>
  );
}