#pragma once

#include "model/circuit.hpp"

#include <string>
#include <vector>

namespace tracemind::placer {

struct Placement {
  std::string component_id;
  int x;
  int y;
  int width;
  int height;
};

struct PlacementStep {
  int iteration;
  std::vector<Placement> placements;
};

struct PlacementResult {
  std::vector<PlacementStep> steps;
  std::vector<Placement> final_positions;
};

// Leiden community detection on the netlist graph.
// Returns community_id[i] for component i (same order as circuit.components).
std::vector<int> DetectCommunities(const model::Circuit& circuit);

// Leiden-seeded simulated annealing placement.
// Leiden groups tightly-connected components; SA minimises HPWL + overlap.
PlacementResult PlaceComponents(const model::Circuit& circuit);

}  // namespace tracemind::placer
