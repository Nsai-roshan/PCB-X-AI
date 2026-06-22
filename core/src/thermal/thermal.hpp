#pragma once

#include "model/circuit.hpp"
#include "placer/placer.hpp"

#include <string>
#include <vector>

namespace tracemind::thermal {

struct ThermalInput {
  int board_width;
  int board_height;
  double ambient_temp_c = 25.0;
  std::vector<double> power_map_w;
  double conductivity    = 0.25;
  double cell_size_m     = 0.001;
  int    max_iterations  = 300;
  double convergence_tol = 1e-4;
};

struct ThermalResult {
  std::vector<double> grid_c;
  double max_temp_c    = 0.0;
  double min_temp_c    = 0.0;
  int    iterations_run = 0;
};

ThermalInput BuildThermalInput(
    const model::Circuit&              circuit,
    const std::vector<placer::Placement>& placements,
    double mcu_power_w     = 0.15,
    double led_power_w     = 0.05,
    double default_power_w = 0.02);

ThermalResult SolveThermal(const ThermalInput& input);

}  // namespace tracemind::thermal