#pragma once

#include "model/circuit.hpp"
#include "placer/placer.hpp"

#include <string>
#include <vector>

namespace tracemind::router {

struct GridPoint {
  int x;
  int y;
};

struct RoutedTrace {
  std::string net_id;
  std::vector<GridPoint> path;
};

struct RoutingStep {
  int routed_count;
  std::vector<RoutedTrace> traces;
};

struct RoutingResult {
  std::vector<RoutingStep> steps;
  std::vector<RoutedTrace> traces;
  int unrouted_net_count = 0;
};

RoutingResult RouteCircuit(
    const model::Circuit& circuit,
    const std::vector<placer::Placement>& placements);

}  // namespace tracemind::router
