#include "thermal/thermal.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <string>

namespace tracemind::thermal {

namespace {
inline int Idx(int x, int y, int w) { return y * w + x; }
}

ThermalInput BuildThermalInput(
    const model::Circuit& circuit,
    const std::vector<placer::Placement>& placements,
    double mcu_power_w,
    double led_power_w,
    double default_power_w) {

  ThermalInput input;
  input.board_width  = circuit.board_width;
  input.board_height = circuit.board_height;
  input.power_map_w.assign(
      static_cast<std::size_t>(circuit.board_width * circuit.board_height), 0.0);

  std::map<std::string, std::string> kind_map;
  for (const auto& comp : circuit.components) kind_map[comp.id] = comp.kind;

  for (const auto& pl : placements) {
    const auto it = kind_map.find(pl.component_id);
    if (it == kind_map.end()) continue;

    std::string lo = it->second;
    std::transform(lo.begin(), lo.end(), lo.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    double total_power;
    if (lo == "mcu" || lo == "ic" || lo == "cpu" || lo == "soc")
      total_power = mcu_power_w;
    else if (lo == "led")
      total_power = led_power_w;
    else if (lo == "battery" || lo == "connector")
      total_power = 0.0;
    else
      total_power = default_power_w;

    const int area = pl.width * pl.height;
    if (area == 0 || total_power == 0.0) continue;
    const double per_cell = total_power / static_cast<double>(area);

    for (int dy = 0; dy < pl.height; ++dy) {
      for (int dx = 0; dx < pl.width; ++dx) {
        const int x = pl.x + dx, y = pl.y + dy;
        if (x < 0 || x >= circuit.board_width ||
            y < 0 || y >= circuit.board_height) continue;
        input.power_map_w[static_cast<std::size_t>(Idx(x, y, circuit.board_width))] += per_cell;
      }
    }
  }
  return input;
}

ThermalResult SolveThermal(const ThermalInput& input) {
  ThermalResult result;
  const int W = input.board_width, H = input.board_height;
  const std::size_t N = static_cast<std::size_t>(W * H);
  result.grid_c.assign(N, input.ambient_temp_c);
  if (W < 2 || H < 2) return result;

  const double h2_over_k = (input.cell_size_m * input.cell_size_m) /
                           std::max(input.conductivity, 1e-9);
  std::vector<double> T(N, input.ambient_temp_c), T_new(N, input.ambient_temp_c);

  for (int iter = 0; iter < input.max_iterations; ++iter) {
    double max_change = 0.0;
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        if (x == 0 || x == W-1 || y == 0 || y == H-1) {
          T_new[static_cast<std::size_t>(Idx(x,y,W))] = input.ambient_temp_c;
          continue;
        }
        const double Q      = input.power_map_w[static_cast<std::size_t>(Idx(x,y,W))];
        const double t_up   = T[static_cast<std::size_t>(Idx(x,y-1,W))];
        const double t_down = T[static_cast<std::size_t>(Idx(x,y+1,W))];
        const double t_left = T[static_cast<std::size_t>(Idx(x-1,y,W))];
        const double t_righ = T[static_cast<std::size_t>(Idx(x+1,y,W))];
        const double t_new  = (t_up + t_down + t_left + t_righ + Q * h2_over_k) / 4.0;
        T_new[static_cast<std::size_t>(Idx(x,y,W))] = t_new;
        max_change = std::max(max_change,
            std::abs(t_new - T[static_cast<std::size_t>(Idx(x,y,W))]));
      }
    }
    T = T_new;
    ++result.iterations_run;
    if (max_change < input.convergence_tol) break;
  }

  result.grid_c    = T;
  result.max_temp_c = *std::max_element(T.begin(), T.end());
  result.min_temp_c = *std::min_element(T.begin(), T.end());
  return result;
}

}  // namespace tracemind::thermal