#include "placer/placer.hpp"

#include <algorithm>
#include <cmath>
#include <climits>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tracemind::placer {

// ---------------------------------------------------------------------------
// Leiden community detection
// ---------------------------------------------------------------------------
namespace {

struct Graph {
  int n = 0;
  // adj[i] = list of (neighbor_index, edge_weight)
  std::vector<std::vector<std::pair<int, double>>> adj;
  std::vector<double> degree;  // sum of edge weights per node
  double total_weight = 0.0;
};

Graph BuildGraph(const model::Circuit& circuit) {
  const int n = static_cast<int>(circuit.components.size());
  Graph g;
  g.n = n;
  g.adj.resize(static_cast<std::size_t>(n));
  g.degree.resize(static_cast<std::size_t>(n), 0.0);

  // Map component id -> index
  std::map<std::string, int> comp_idx;
  for (int i = 0; i < n; ++i) {
    comp_idx[circuit.components[static_cast<std::size_t>(i)].id] = i;
  }

  // Edge weight matrix (dense for small N)
  std::vector<std::vector<double>> w(static_cast<std::size_t>(n),
                                     std::vector<double>(static_cast<std::size_t>(n), 0.0));

  for (const auto& net : circuit.nets) {
    // All pairs of components sharing this net get edge weight +1
    std::vector<int> members;
    for (const auto& conn : net.connections) {
      const auto it = comp_idx.find(conn.component);
      if (it != comp_idx.end()) members.push_back(it->second);
    }
    for (std::size_t a = 0; a < members.size(); ++a) {
      for (std::size_t b = a + 1; b < members.size(); ++b) {
        w[static_cast<std::size_t>(members[a])][static_cast<std::size_t>(members[b])] += 1.0;
        w[static_cast<std::size_t>(members[b])][static_cast<std::size_t>(members[a])] += 1.0;
      }
    }
  }

  // Convert to adjacency list
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      if (i != j && w[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] > 0.0) {
        g.adj[static_cast<std::size_t>(i)].emplace_back(j,
            w[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]);
        g.degree[static_cast<std::size_t>(i)] +=
            w[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
      }
    }
    g.total_weight += g.degree[static_cast<std::size_t>(i)];
  }
  g.total_weight /= 2.0;  // each edge counted twice above

  return g;
}

// Leiden local-moving phase: greedy modularity maximisation.
// community[i] = community id for node i.
bool LocalMovingPhase(const Graph& g, std::vector<int>& community, double resolution) {
  const int n = g.n;
  const double m = g.total_weight == 0.0 ? 1.0 : g.total_weight;
  bool improved = false;

  for (int v = 0; v < n; ++v) {
    // Sum of edge weights from v to each community
    std::map<int, double> k_v_c;
    for (const auto& [u, w] : g.adj[static_cast<std::size_t>(v)]) {
      k_v_c[community[static_cast<std::size_t>(u)]] += w;
    }

    // Sum of degrees in current community (excluding v)
    std::map<int, double> sigma_c;
    for (int u = 0; u < n; ++u) {
      if (u == v) continue;
      sigma_c[community[static_cast<std::size_t>(u)]] +=
          g.degree[static_cast<std::size_t>(u)];
    }

    const int cur_c = community[static_cast<std::size_t>(v)];
    const double k_v = g.degree[static_cast<std::size_t>(v)];
    const double k_v_cur = k_v_c.count(cur_c) ? k_v_c.at(cur_c) : 0.0;
    const double sig_cur = sigma_c.count(cur_c) ? sigma_c.at(cur_c) : 0.0;

    int best_c = cur_c;
    double best_gain = 0.0;

    for (const auto& [cand_c, k_v_cand] : k_v_c) {
      if (cand_c == cur_c) continue;
      const double sig_cand = sigma_c.count(cand_c) ? sigma_c.at(cand_c) : 0.0;
      // ΔQ = (k_v_cand - k_v_cur)/m + resolution*k_v*(sig_cur - sig_cand)/(2m^2)
      const double gain = (k_v_cand - k_v_cur) / m +
                          resolution * k_v * (sig_cur - sig_cand) / (2.0 * m * m);
      if (gain > best_gain) {
        best_gain = gain;
        best_c = cand_c;
      }
    }

    if (best_c != cur_c) {
      community[static_cast<std::size_t>(v)] = best_c;
      improved = true;
    }
  }

  return improved;
}

// Normalise community ids to 0..k-1
std::vector<int> NormaliseCommunities(const std::vector<int>& raw) {
  std::map<int, int> remap;
  int next_id = 0;
  std::vector<int> result(raw.size());
  for (std::size_t i = 0; i < raw.size(); ++i) {
    auto [it, inserted] = remap.emplace(raw[i], next_id);
    if (inserted) ++next_id;
    result[i] = it->second;
  }
  return result;
}

}  // namespace

std::vector<int> DetectCommunities(const model::Circuit& circuit) {
  const int n = static_cast<int>(circuit.components.size());
  if (n == 0) return {};

  const Graph g = BuildGraph(circuit);
  std::vector<int> community(static_cast<std::size_t>(n));
  std::iota(community.begin(), community.end(), 0);

  // Leiden: iterate local-moving phases until stable
  const double resolution = 1.0;
  bool improved = true;
  int max_rounds = 20;
  while (improved && max_rounds-- > 0) {
    improved = LocalMovingPhase(g, community, resolution);
  }

  return NormaliseCommunities(community);
}

// ---------------------------------------------------------------------------
// Simulated Annealing placement
// ---------------------------------------------------------------------------
namespace {

using Pos = std::pair<int, int>;

// Map component id -> index in circuit.components
std::map<std::string, int> MakeCompIndex(const model::Circuit& circuit) {
  std::map<std::string, int> idx;
  for (int i = 0; i < static_cast<int>(circuit.components.size()); ++i) {
    idx[circuit.components[static_cast<std::size_t>(i)].id] = i;
  }
  return idx;
}

// Half-perimeter wirelength over all nets
double HPWL(const model::Circuit& circuit,
            const std::vector<Pos>& pos,
            const std::map<std::string, int>& idx) {
  double total = 0.0;
  for (const auto& net : circuit.nets) {
    int xmin = INT_MAX, xmax = INT_MIN, ymin = INT_MAX, ymax = INT_MIN;
    for (const auto& conn : net.connections) {
      const auto it = idx.find(conn.component);
      if (it == idx.end()) continue;
      const int i = it->second;
      const int cx = pos[static_cast<std::size_t>(i)].first +
                     circuit.components[static_cast<std::size_t>(i)].width / 2;
      const int cy = pos[static_cast<std::size_t>(i)].second +
                     circuit.components[static_cast<std::size_t>(i)].height / 2;
      xmin = std::min(xmin, cx); xmax = std::max(xmax, cx);
      ymin = std::min(ymin, cy); ymax = std::max(ymax, cy);
    }
    if (xmin <= xmax) total += (xmax - xmin) + (ymax - ymin);
  }
  return total;
}

// Overlap area between all component pairs (penalty)
double OverlapPenalty(const model::Circuit& circuit, const std::vector<Pos>& pos) {
  const int n = static_cast<int>(circuit.components.size());
  double pen = 0.0;
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      const int ax = pos[static_cast<std::size_t>(i)].first;
      const int ay = pos[static_cast<std::size_t>(i)].second;
      const int aw = circuit.components[static_cast<std::size_t>(i)].width;
      const int ah = circuit.components[static_cast<std::size_t>(i)].height;
      const int bx = pos[static_cast<std::size_t>(j)].first;
      const int by = pos[static_cast<std::size_t>(j)].second;
      const int bw = circuit.components[static_cast<std::size_t>(j)].width;
      const int bh = circuit.components[static_cast<std::size_t>(j)].height;
      const int ox = std::max(0, std::min(ax + aw, bx + bw) - std::max(ax, bx));
      const int oy = std::max(0, std::min(ay + ah, by + bh) - std::max(ay, by));
      pen += static_cast<double>(ox * oy);
    }
  }
  return pen;
}

// Community cohesion: penalise separation of community members
double CohesionPenalty(const std::vector<Pos>& pos,
                       const std::vector<int>& comm) {
  const int n = static_cast<int>(pos.size());
  double pen = 0.0;
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      if (comm[static_cast<std::size_t>(i)] !=
          comm[static_cast<std::size_t>(j)]) continue;
      const double dx = pos[static_cast<std::size_t>(i)].first -
                        pos[static_cast<std::size_t>(j)].first;
      const double dy = pos[static_cast<std::size_t>(i)].second -
                        pos[static_cast<std::size_t>(j)].second;
      pen += std::sqrt(dx * dx + dy * dy);
    }
  }
  return pen;
}

double Cost(const model::Circuit& circuit,
            const std::vector<Pos>& pos,
            const std::vector<int>& comm,
            const std::map<std::string, int>& idx) {
  return HPWL(circuit, pos, idx) +
         12.0 * OverlapPenalty(circuit, pos) +
         0.4  * CohesionPenalty(pos, comm);
}

// Clamp position so component stays inside the board
Pos Clamp(Pos p, int w, int h, int bw, int bh) {
  p.first  = std::max(0, std::min(p.first,  bw - w));
  p.second = std::max(0, std::min(p.second, bh - h));
  return p;
}

// Seed initial positions using community zones
std::vector<Pos> CommunitySeededInit(const model::Circuit& circuit,
                                     const std::vector<int>& comm) {
  const int n = static_cast<int>(circuit.components.size());
  const int bw = circuit.board_width;
  const int bh = circuit.board_height;

  // Count communities
  int num_comm = 0;
  for (int c : comm) num_comm = std::max(num_comm, c + 1);
  if (num_comm == 0) num_comm = 1;

  // Divide board into zones per community (horizontal strips)
  const int zone_w = std::max(1, bw / num_comm);

  // Cursor per community
  std::vector<Pos> cursor(static_cast<std::size_t>(num_comm), {0, 0});
  for (int c = 0; c < num_comm; ++c) {
    cursor[static_cast<std::size_t>(c)] = {c * zone_w + 1, 1};
  }
  std::vector<int> row_height(static_cast<std::size_t>(num_comm), 0);

  std::vector<Pos> pos(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    const int c = comm[static_cast<std::size_t>(i)];
    const int cw = circuit.components[static_cast<std::size_t>(i)].width;
    const int ch = circuit.components[static_cast<std::size_t>(i)].height;
    const int zone_end_x = (c + 1) * zone_w;

    auto& cur = cursor[static_cast<std::size_t>(c)];
    auto& rh  = row_height[static_cast<std::size_t>(c)];

    if (cur.first + cw > zone_end_x) {
      cur.first  = c * zone_w + 1;
      cur.second += rh + 1;
      rh = 0;
    }

    pos[static_cast<std::size_t>(i)] =
        Clamp({cur.first, cur.second}, cw, ch, bw, bh);
    cur.first += cw + 1;
    rh = std::max(rh, ch);
  }

  return pos;
}

Placement MakePlacement(const model::Component& comp, const Pos& pos) {
  return {comp.id, pos.first, pos.second, comp.width, comp.height};
}

}  // namespace

PlacementResult PlaceComponents(const model::Circuit& circuit) {
  const int n = static_cast<int>(circuit.components.size());
  PlacementResult result;

  if (n == 0) return result;

  // 1. Leiden community detection
  const std::vector<int> comm = DetectCommunities(circuit);
  const std::map<std::string, int> idx = MakeCompIndex(circuit);

  // 2. Community-seeded initial placement
  std::vector<Pos> pos = CommunitySeededInit(circuit, comm);

  // 3. Simulated annealing
  std::mt19937 rng(42);  // ponytail: fixed seed for reproducible demos
  std::uniform_int_distribution<int> pick_comp(0, n - 1);
  std::uniform_real_distribution<double> uniform(0.0, 1.0);

  const int bw = circuit.board_width;
  const int bh = circuit.board_height;

  double temperature = 0.5 * Cost(circuit, pos, comm, idx) + 1e-6;
  const double cooling = 0.95;
  const double t_min   = 1e-3;
  // ~60 iterations captured as animation steps (record every epoch)
  const int epochs_per_step = std::max(1, n * 4);
  int step_iter = 0;

  while (temperature > t_min) {
    for (int iter = 0; iter < epochs_per_step; ++iter) {
      const int a = pick_comp(rng);
      const int b = pick_comp(rng);

      std::vector<Pos> candidate = pos;

      if (a == b) {
        // Random nudge
        std::uniform_int_distribution<int> nudge(-2, 2);
        candidate[static_cast<std::size_t>(a)].first  += nudge(rng);
        candidate[static_cast<std::size_t>(a)].second += nudge(rng);
        candidate[static_cast<std::size_t>(a)] =
            Clamp(candidate[static_cast<std::size_t>(a)],
                  circuit.components[static_cast<std::size_t>(a)].width,
                  circuit.components[static_cast<std::size_t>(a)].height, bw, bh);
      } else {
        // Swap positions of two components
        std::swap(candidate[static_cast<std::size_t>(a)],
                  candidate[static_cast<std::size_t>(b)]);
        candidate[static_cast<std::size_t>(a)] =
            Clamp(candidate[static_cast<std::size_t>(a)],
                  circuit.components[static_cast<std::size_t>(a)].width,
                  circuit.components[static_cast<std::size_t>(a)].height, bw, bh);
        candidate[static_cast<std::size_t>(b)] =
            Clamp(candidate[static_cast<std::size_t>(b)],
                  circuit.components[static_cast<std::size_t>(b)].width,
                  circuit.components[static_cast<std::size_t>(b)].height, bw, bh);
      }

      const double delta = Cost(circuit, candidate, comm, idx) -
                           Cost(circuit, pos, comm, idx);
      if (delta < 0.0 || uniform(rng) < std::exp(-delta / temperature)) {
        pos = candidate;
      }
    }

    // Capture a placement step for animation every SA epoch
    PlacementStep step;
    step.iteration = step_iter++;
    for (int i = 0; i < n; ++i) {
      step.placements.push_back(
          MakePlacement(circuit.components[static_cast<std::size_t>(i)],
                        pos[static_cast<std::size_t>(i)]));
    }
    result.steps.push_back(std::move(step));

    temperature *= cooling;
  }

  // Final positions
  for (int i = 0; i < n; ++i) {
    result.final_positions.push_back(
        MakePlacement(circuit.components[static_cast<std::size_t>(i)],
                      pos[static_cast<std::size_t>(i)]));
  }

  return result;
}

}  // namespace tracemind::placer
