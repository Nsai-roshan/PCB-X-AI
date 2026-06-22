#include "engine/engine.hpp"

#include "irdrop/ir_solver.hpp"
#include "placer/placer.hpp"
#include "router/router.hpp"
#include "thermal/thermal.hpp"

#include <algorithm>
#include <map>
#include <numeric>
#include <string>
#include <vector>

namespace tracemind::engine {
namespace {

nlohmann::json PlacementToJson(const placer::Placement& p) {
  return {{"componentId", p.component_id},
          {"x", p.x}, {"y", p.y},
          {"width", p.width}, {"height", p.height}};
}

nlohmann::json TraceToJson(const router::RoutedTrace& trace) {
  nlohmann::json path = nlohmann::json::array();
  for (const auto& pt : trace.path)
    path.push_back({{"x", pt.x}, {"y", pt.y}});
  return {{"netId", trace.net_id}, {"path", path}};
}

int TotalTraceLength(const std::vector<router::RoutedTrace>& traces) {
  int total = 0;
  for (const auto& t : traces) total += static_cast<int>(t.path.size());
  return total;
}

double BoardUtilization(const model::Circuit& c,
                        const std::vector<model::Component>& comps) {
  const int board_area = c.board_width * c.board_height;
  if (board_area == 0) return 0.0;
  int comp_area = 0;
  for (const auto& comp : comps) comp_area += comp.width * comp.height;
  return static_cast<double>(comp_area) * 100.0 / static_cast<double>(board_area);
}

nlohmann::json GridToJson(const std::vector<double>& grid) {
  nlohmann::json arr = nlohmann::json::array();
  for (double v : grid) arr.push_back(v);
  return arr;
}

nlohmann::json BuildMetrics(
    const model::Circuit& circuit,
    const router::RoutingResult& routing,
    const irdrop::IrDropResult& ir,
    const thermal::ThermalResult& th) {
  return {
      {"componentCount",          static_cast<int>(circuit.components.size())},
      {"netCount",                static_cast<int>(circuit.nets.size())},
      {"routedNetCount",          static_cast<int>(routing.traces.size())},
      {"unroutedNetCount",        routing.unrouted_net_count},
      {"totalTraceLength",        TotalTraceLength(routing.traces)},
      {"boardUtilizationPercent", BoardUtilization(circuit, circuit.components)},
      {"maxIrDropMv",             ir.max_drop_mv},
      {"irViolationCount",        ir.violation_count},
      {"irSolverConverged",       ir.solver_converged},
      {"maxTemperatureC",         th.max_temp_c},
      {"minTemperatureC",         th.min_temp_c},
      {"thermalIterations",       th.iterations_run},
      {"drcViolationCount",       routing.unrouted_net_count},
  };
}

nlohmann::json BuildCommentary(
    const model::Circuit& circuit,
    const placer::PlacementResult& placement,
    const router::RoutingResult& routing,
    const irdrop::IrDropResult& ir,
    const thermal::ThermalResult& th) {
  nlohmann::json commentary = nlohmann::json::array();
  commentary.push_back({{"stage", "run"},
    {"text", "Loaded " + std::to_string(circuit.components.size()) +
     " components and " + std::to_string(circuit.nets.size()) + " nets."}});
  commentary.push_back({{"stage", "placement"},
    {"text", "Leiden detected communities; SA converged in " +
     std::to_string(placement.steps.size()) + " epochs."}});
  commentary.push_back({{"stage", "routing"},
    {"text", "Routed " + std::to_string(routing.traces.size()) +
     " nets. " + std::to_string(routing.unrouted_net_count) + " unrouted."}});
  commentary.push_back({{"stage", "analysis"},
    {"text", "IR drop max: " +
     std::to_string(static_cast<int>(ir.max_drop_mv)) + " mV (" +
     std::to_string(ir.violation_count) + " violations). Max temp: " +
     std::to_string(static_cast<int>(th.max_temp_c)) + " degC."}});
  return commentary;
}

}  // namespace

nlohmann::json BuildRunJson(const model::Circuit& circuit, const RunParams& params) {
  // 1. Leiden + SA placement
  const auto placement = placer::PlaceComponents(circuit);

  // 2. BFS routing
  const auto routing = router::RouteCircuit(circuit, placement.final_positions);

  // 3. IR drop (CG sparse solver)
  const std::string power_net = irdrop::FindPowerNetId(circuit);
  irdrop::IrDropResult ir;
  if (!power_net.empty()) {
    auto ir_input = irdrop::BuildIrDropInput(
        circuit, placement.final_positions, routing, power_net);
    ir_input.trace_resistance_per_cell = params.trace_resistance_per_cell;
    ir_input.load_current_per_pin_a    = params.load_current_per_pin_a;
    ir_input.violation_threshold_mv    = params.violation_threshold_mv;
    ir_input.supply_voltage            = params.supply_voltage;
    ir = irdrop::SolveIrDrop(ir_input);
  } else {
    ir.grid_mv.assign(
        static_cast<std::size_t>(circuit.board_width * circuit.board_height), 0.0);
  }

  // 4. Thermal FDM
  const auto th_input = thermal::BuildThermalInput(
      circuit, placement.final_positions,
      params.mcu_power_w, params.led_power_w, params.default_power_w);
  const auto th = thermal::SolveThermal(th_input);

  // 5. Metrics + output JSON
  const auto metrics = BuildMetrics(circuit, routing, ir, th);

  nlohmann::json components = nlohmann::json::array();
  for (std::size_t i = 0; i < circuit.components.size(); ++i) {
    const auto& comp = circuit.components[i];
    nlohmann::json cj{{"id", comp.id}, {"kind", comp.kind},
                      {"width", comp.width}, {"height", comp.height}};
    if (i < placement.final_positions.size()) {
      cj["x"] = placement.final_positions[i].x;
      cj["y"] = placement.final_positions[i].y;
    }
    nlohmann::json pins = nlohmann::json::array();
    for (const auto& pin : comp.pins)
      pins.push_back({{"id", pin.id}, {"x", pin.x}, {"y", pin.y}});
    cj["pins"] = pins;
    components.push_back(cj);
  }

  nlohmann::json traces = nlohmann::json::array();
  for (const auto& trace : routing.traces) traces.push_back(TraceToJson(trace));

  nlohmann::json events = nlohmann::json::array();
  events.push_back({{"stage", "run"}, {"type", "run_started"},
    {"payload", {{"componentCount", circuit.components.size()},
                 {"netCount", circuit.nets.size()}}}});
  for (const auto& step : placement.steps) {
    nlohmann::json pls = nlohmann::json::array();
    for (const auto& pl : step.placements) pls.push_back(PlacementToJson(pl));
    events.push_back({{"stage", "placement"}, {"type", "placement_step"},
      {"payload", {{"iteration", step.iteration}, {"placements", pls}}}});
  }
  events.push_back({{"stage", "placement"}, {"type", "placement_complete"},
    {"payload", {{"placementCount", placement.final_positions.size()}}}});
  for (const auto& step : routing.steps) {
    nlohmann::json rt = nlohmann::json::array();
    for (const auto& t : step.traces) rt.push_back(TraceToJson(t));
    events.push_back({{"stage", "routing"}, {"type", "routing_step"},
      {"payload", {{"routedCount", step.routed_count}, {"traces", rt}}}});
  }
  events.push_back({{"stage", "routing"}, {"type", "routing_complete"},
    {"payload", {{"routedNetCount", routing.traces.size()},
                 {"unroutedNetCount", routing.unrouted_net_count}}}});
  events.push_back({{"stage", "analysis"}, {"type", "analysis_complete"},
    {"payload", metrics}});
  events.push_back({{"stage", "run"}, {"type", "run_complete"},
    {"payload", {{"status", "ok"}}}});

  return {
    {"board",       {{"width", circuit.board_width}, {"height", circuit.board_height}}},
    {"components",  components},
    {"traces",      traces},
    {"metrics",     metrics},
    {"irDropGrid",  GridToJson(ir.grid_mv)},
    {"thermalGrid", GridToJson(th.grid_c)},
    {"events",      events},
    {"commentary",  BuildCommentary(circuit, placement, routing, ir, th)},
  };
}

}  // namespace tracemind::engine