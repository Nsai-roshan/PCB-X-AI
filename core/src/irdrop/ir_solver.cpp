#include "irdrop/ir_solver.hpp"

#include <Eigen/Sparse>
#include <Eigen/IterativeLinearSolvers>

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tracemind::irdrop {

namespace {

using SparseMatrix = Eigen::SparseMatrix<double>;
using Triplet = Eigen::Triplet<double>;

std::string ToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

bool IsPowerNetName(const std::string& id) {
  const std::string lo = ToLower(id);
  return lo == "vcc" || lo == "vdd" || lo == "pwr" ||
         lo == "3v3" || lo == "5v"  || lo == "3.3v" ||
         lo.rfind("vcc", 0) == 0   || lo.rfind("vdd", 0) == 0 ||
         lo.rfind("pwr", 0) == 0;
}

router::GridPoint ResolvePin(
    const model::Connection& conn,
    const std::vector<model::Component>& components,
    const std::vector<placer::Placement>& placements) {
  for (std::size_t i = 0; i < components.size(); ++i) {
    if (components[i].id != conn.component) continue;
    for (const auto& pin : components[i].pins) {
      if (pin.id != conn.pin) continue;
      if (i < placements.size()) {
        return {placements[i].x + pin.x, placements[i].y + pin.y};
      }
    }
  }
  return {-1, -1};
}

}  // namespace

std::string FindPowerNetId(const model::Circuit& circuit) {
  for (const auto& net : circuit.nets) {
    if (IsPowerNetName(net.id)) return net.id;
  }
  return "";
}

IrDropInput BuildIrDropInput(
    const model::Circuit& circuit,
    const std::vector<placer::Placement>& placements,
    const router::RoutingResult& routing,
    const std::string& power_net_id) {
  IrDropInput input;
  input.board_width  = circuit.board_width;
  input.board_height = circuit.board_height;

  // Find routed trace for this net
  for (const auto& trace : routing.traces) {
    if (trace.net_id == power_net_id) {
      for (const auto& pt : trace.path) {
        input.trace_cells.push_back(pt);
      }
    }
  }

  // Find the net definition
  for (const auto& net : circuit.nets) {
    if (net.id != power_net_id) continue;

    // First connection = source / supply
    if (!net.connections.empty()) {
      const auto sp = ResolvePin(net.connections.front(), circuit.components, placements);
      if (sp.x >= 0) input.supply_pins.push_back(sp);
    }

    // Remaining connections = loads
    for (std::size_t i = 1; i < net.connections.size(); ++i) {
      const auto lp = ResolvePin(net.connections[i], circuit.components, placements);
      if (lp.x >= 0) input.load_pins.push_back(lp);
    }
    break;
  }

  // Snap each load pin to the nearest trace cell (router only connects 2
  // endpoints per net; other pins may not be on the routed path).
  if (!input.trace_cells.empty()) {
    for (auto& lp : input.load_pins) {
      // Check if already on a trace cell
      bool on_trace = false;
      for (const auto& tc : input.trace_cells) {
        if (tc.x == lp.x && tc.y == lp.y) { on_trace = true; break; }
      }
      if (on_trace) continue;
      // Find nearest trace cell (Manhattan for speed)
      double min_dist = 1e9;
      router::GridPoint nearest = input.trace_cells.front();
      for (const auto& tc : input.trace_cells) {
        const double d = std::abs(tc.x - lp.x) + std::abs(tc.y - lp.y);
        if (d < min_dist) { min_dist = d; nearest = tc; }
      }
      lp = nearest;
    }
  }

  return input;
}

IrDropResult SolveIrDrop(const IrDropInput& input) {
  IrDropResult result;
  result.grid_mv.assign(
      static_cast<std::size_t>(input.board_width * input.board_height), 0.0);

  if (input.trace_cells.empty() || input.supply_pins.empty()) {
    return result;  // nothing to solve
  }

  const double G = 1.0 / input.trace_resistance_per_cell;
  // Large conductance to enforce supply voltage at pad nodes
  const double G_pad = G * 1e6;

  // Map (x,y) trace cells to solver node indices
  std::map<std::pair<int,int>, int> cell_to_node;
  // Deduplicate trace cells
  std::set<std::pair<int,int>> trace_set;
  for (const auto& pt : input.trace_cells) {
    trace_set.insert({pt.x, pt.y});
  }
  // Supply pin cells are always included even if the router didn't tag them
  for (const auto& pt : input.supply_pins) {
    trace_set.insert({pt.x, pt.y});
  }
  for (const auto& pt : input.load_pins) {
    trace_set.insert({pt.x, pt.y});
  }

  int node_count = 0;
  for (const auto& cell : trace_set) {
    cell_to_node[cell] = node_count++;
  }

  if (node_count == 0) return result;

  std::vector<Triplet> triplets;
  triplets.reserve(static_cast<std::size_t>(node_count) * 5);
  Eigen::VectorXd rhs = Eigen::VectorXd::Zero(node_count);

  const std::array<std::pair<int,int>, 4> kDirs{{{1,0},{-1,0},{0,1},{0,-1}}};

  for (const auto& [cell, idx] : cell_to_node) {
    double diag = 0.0;
    for (const auto& dir : kDirs) {
      const std::pair<int,int> neighbor{cell.first + dir.first,
                                        cell.second + dir.second};
      const auto it = cell_to_node.find(neighbor);
      if (it == cell_to_node.end()) continue;
      diag += G;
      triplets.emplace_back(idx, it->second, -G);
    }
    triplets.emplace_back(idx, idx, diag);
  }

  // Supply pads: add large conductance to ground-referenced supply
  for (const auto& pt : input.supply_pins) {
    const auto it = cell_to_node.find({pt.x, pt.y});
    if (it == cell_to_node.end()) continue;
    const int idx = it->second;
    triplets.emplace_back(idx, idx, G_pad);
    rhs[idx] += G_pad * input.supply_voltage;
  }

  // Load pins: inject current drain
  for (const auto& pt : input.load_pins) {
    const auto it = cell_to_node.find({pt.x, pt.y});
    if (it == cell_to_node.end()) continue;
    rhs[it->second] -= input.load_current_per_pin_a;
  }

  SparseMatrix mat(node_count, node_count);
  mat.setFromTriplets(triplets.begin(), triplets.end());
  mat.makeCompressed();

  Eigen::ConjugateGradient<SparseMatrix, Eigen::Lower | Eigen::Upper,
                           Eigen::DiagonalPreconditioner<double>> solver;
  solver.setMaxIterations(2000);
  solver.setTolerance(1e-10);
  solver.compute(mat);

  if (solver.info() != Eigen::Success) return result;

  const Eigen::VectorXd voltages = solver.solve(rhs);
  result.solver_converged = (solver.info() == Eigen::Success);

  // Map node voltages back to the W*H grid
  for (const auto& [cell, idx] : cell_to_node) {
    const double v = voltages[idx];
    const double drop_mv = std::max(0.0, input.supply_voltage - v) * 1000.0;
    const int grid_idx = cell.second * input.board_width + cell.first;
    if (grid_idx >= 0 &&
        grid_idx < static_cast<int>(result.grid_mv.size())) {
      result.grid_mv[static_cast<std::size_t>(grid_idx)] = drop_mv;
    }
    result.max_drop_mv = std::max(result.max_drop_mv, drop_mv);
    if (drop_mv > input.violation_threshold_mv) {
      ++result.violation_count;
    }
  }

  return result;
}

}  // namespace tracemind::irdrop
