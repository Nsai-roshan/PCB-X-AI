#pragma once

#include "model/circuit.hpp"
#include "placer/placer.hpp"
#include "router/router.hpp"

#include <string>
#include <vector>

namespace tracemind::irdrop {

struct IrDropInput {
  int board_width;
  int board_height;
  std::vector<router::GridPoint> trace_cells;
  std::vector<router::GridPoint> supply_pins;
  std::vector<router::GridPoint> load_pins;
  double supply_voltage = 3.3;
  double trace_resistance_per_cell = 0.05;
  double load_current_per_pin_a = 0.01;
  double violation_threshold_mv = 30.0;
};

struct IrDropResult {
  std::vector<double> grid_mv;
  double max_drop_mv = 0.0;
  int violation_count = 0;
  bool solver_converged = false;
};

IrDropInput BuildIrDropInput(
    const model::Circuit& circuit,
    const std::vector<placer::Placement>& placements,
    const router::RoutingResult& routing,
    const std::string& power_net_id);

IrDropResult SolveIrDrop(const IrDropInput& input);

std::string FindPowerNetId(const model::Circuit& circuit);

}  // namespace tracemind::irdrop
