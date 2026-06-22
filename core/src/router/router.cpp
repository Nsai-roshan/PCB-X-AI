#include "router/router.hpp"

#include <array>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tracemind::router {
namespace {

using Coordinate = std::pair<int, int>;

std::map<std::string, placer::Placement> BuildPlacementIndex(
    const std::vector<placer::Placement>& placements) {
  std::map<std::string, placer::Placement> index;
  for (const auto& placement : placements) {
    index.emplace(placement.component_id, placement);
  }
  return index;
}

std::map<std::string, model::Component> BuildComponentIndex(
    const model::Circuit& circuit) {
  std::map<std::string, model::Component> index;
  for (const auto& component : circuit.components) {
    index.emplace(component.id, component);
  }
  return index;
}

std::optional<GridPoint> ResolvePinLocation(
    const model::Connection& connection,
    const std::map<std::string, model::Component>& component_index,
    const std::map<std::string, placer::Placement>& placement_index) {
  const auto component_it = component_index.find(connection.component);
  const auto placement_it = placement_index.find(connection.component);

  if (component_it == component_index.end() || placement_it == placement_index.end()) {
    return std::nullopt;
  }

  for (const auto& pin : component_it->second.pins) {
    if (pin.id == connection.pin) {
      return GridPoint{
          placement_it->second.x + pin.x,
          placement_it->second.y + pin.y,
      };
    }
  }

  return std::nullopt;
}

bool IsInsideBoard(const model::Circuit& circuit, int x, int y) {
  return x >= 0 && y >= 0 && x < circuit.board_width && y < circuit.board_height;
}

bool IsComponentCellBlocked(
    const std::vector<placer::Placement>& placements,
    int x,
    int y,
    const std::set<std::string>& allowed_components) {
  for (const auto& placement : placements) {
    if (allowed_components.count(placement.component_id) > 0) {
      continue;
    }

    const bool inside_x =
        x >= placement.x && x < placement.x + placement.width;
    const bool inside_y =
        y >= placement.y && y < placement.y + placement.height;
    if (inside_x && inside_y) {
      return true;
    }
  }

  return false;
}

std::optional<std::vector<GridPoint>> FindPath(
    const model::Circuit& circuit,
    const std::vector<placer::Placement>& placements,
    const std::set<Coordinate>& occupied_trace_cells,
    const std::set<std::string>& allowed_components,
    const GridPoint& start,
    const GridPoint& goal) {
  std::queue<Coordinate> frontier;
  std::set<Coordinate> visited;
  std::map<Coordinate, Coordinate> parents;

  const Coordinate start_key{start.x, start.y};
  const Coordinate goal_key{goal.x, goal.y};

  frontier.push(start_key);
  visited.insert(start_key);

  constexpr std::array<Coordinate, 4> kDirections{
      Coordinate{1, 0},
      Coordinate{-1, 0},
      Coordinate{0, 1},
      Coordinate{0, -1},
  };

  while (!frontier.empty()) {
    const auto current = frontier.front();
    frontier.pop();

    if (current == goal_key) {
      break;
    }

    for (const auto& direction : kDirections) {
      const Coordinate next{current.first + direction.first,
                            current.second + direction.second};

      if (!IsInsideBoard(circuit, next.first, next.second)) {
        continue;
      }

      if (visited.count(next) > 0) {
        continue;
      }

      if (next != goal_key &&
          occupied_trace_cells.count(next) > 0) {
        continue;
      }

      if (next != goal_key &&
          next != start_key &&
          IsComponentCellBlocked(
              placements, next.first, next.second, allowed_components)) {
        continue;
      }

      visited.insert(next);
      parents.emplace(next, current);
      frontier.push(next);
    }
  }

  if (visited.count(goal_key) == 0) {
    return std::nullopt;
  }

  std::vector<GridPoint> path;
  Coordinate current = goal_key;
  while (true) {
    path.push_back(GridPoint{current.first, current.second});
    if (current == start_key) {
      break;
    }
    current = parents.at(current);
  }

  std::reverse(path.begin(), path.end());
  return path;
}

}  // namespace

RoutingResult RouteCircuit(
    const model::Circuit& circuit,
    const std::vector<placer::Placement>& placements) {
  RoutingResult result;
  const auto component_index = BuildComponentIndex(circuit);
  const auto placement_index = BuildPlacementIndex(placements);
  std::set<Coordinate> occupied_trace_cells;

  for (const auto& net : circuit.nets) {
    if (net.connections.size() < 2) {
      continue;
    }

    const auto start =
        ResolvePinLocation(net.connections.front(), component_index, placement_index);
    const auto end =
        ResolvePinLocation(net.connections.back(), component_index, placement_index);

    if (!start.has_value() || !end.has_value()) {
      result.unrouted_net_count += 1;
      continue;
    }

    const std::set<std::string> allowed_components{
        net.connections.front().component,
        net.connections.back().component,
    };

    const auto path = FindPath(
        circuit,
        placements,
        occupied_trace_cells,
        allowed_components,
        *start,
        *end);

    if (!path.has_value()) {
      result.unrouted_net_count += 1;
      continue;
    }

    RoutedTrace trace{};
    trace.net_id = net.id;
    trace.path = *path;
    result.traces.push_back(trace);

    for (const auto& point : trace.path) {
      occupied_trace_cells.emplace(point.x, point.y);
    }

    RoutingStep step{};
    step.routed_count = static_cast<int>(result.traces.size());
    step.traces = result.traces;
    result.steps.push_back(step);
  }

  return result;
}

}  // namespace tracemind::router
