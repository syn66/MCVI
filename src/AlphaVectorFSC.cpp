#include "AlphaVectorFSC.h"

namespace MCVI {

int64_t AlphaVectorFSC::GetEdgeValue(int64_t nI, int64_t observation) const {
  const std::unordered_map<int64_t, int64_t>& m = _edges[nI];
  const auto it = m.find(observation);
  if (it != m.cend()) return it->second;
  return -1;
}

const std::unordered_map<int64_t, int64_t>& AlphaVectorFSC::GetEdges(
    int64_t nI) const {
  return _edges[nI];
}

int64_t AlphaVectorFSC::AddNode(const AlphaVectorNode& node) {
  _nodes.emplace_back(node);
  return _nodes.size() - 1;
}

void AlphaVectorFSC::UpdateEdge(int64_t nI, int64_t o, int64_t nI_new) {
  _edges[nI][o] = nI_new;
}

void AlphaVectorFSC::UpdateEdge(
    int64_t nI, const std::unordered_map<int64_t, int64_t>& edges) {
  _edges[nI] = edges;
}

void AlphaVectorFSC::GenerateGraphviz(
    std::ostream& ofs, const std::vector<std::string>& actions,
    const std::vector<std::string>& observations) const {
  if (!ofs) throw std::logic_error("Invalid output file");

  ofs << "digraph AlphaVectorFSC {" << std::endl;

  ofs << "node [shape=circle];" << std::endl;

  // Loop through each node
  for (int64_t i = 0; i < NumNodes(); ++i) {
    const AlphaVectorNode& node = GetNode(i);

    std::string action = actions.empty() ? std::to_string(node.GetBestAction())
                                         : actions[node.GetBestAction()];
    ofs << " n" << i << " [label=<<B>" << i << "</B><BR/>a: " << action
        << "<BR/>V: " << node.V_node() << ">";
    if (i == GetStartNodeIndex()) {  // highlight start node with double outline
      ofs << ", penwidth=3";
    }
    ofs << "];" << std::endl;

    // Loop through edges from this node
    for (const auto& edge : _edges[i]) {
      std::string observation = observations.empty()
                                    ? std::to_string(edge.first)
                                    : observations[edge.first];
      int64_t target_node = edge.second;
      ofs << "n" << i << " -> n" << target_node << " [label=<" << observation
          << ">];";
    }
    ofs << std::endl;
  }

  ofs << "}" << std::endl;
}

double AlphaVectorFSC::SimulateTrajectory(int64_t nI, const State& state,
                                          int64_t max_depth, double R_lower,
                                          SimInterface* pomdp) {
  const double gamma = pomdp->GetDiscount();
  double V_n_s = 0.0;
  int64_t nI_current = nI;
  State curr_state = state;
  for (int64_t step = 0; step < max_depth; ++step) {
    if (nI_current == -1) {
      const double reward = std::pow(gamma, max_depth) * R_lower;
      V_n_s += std::pow(gamma, step) * reward;
      break;
    }

    const int64_t action = GetNode(nI_current).GetBestAction();
    const auto [sNext, obs, reward, done] = pomdp->Step(curr_state, action);
    nI_current = GetEdgeValue(nI_current, obs);
    V_n_s += std::pow(gamma, step) * reward;
    if (done) break;
    curr_state = sNext;
  }

  return V_n_s;
}

double AlphaVectorFSC::GetNodeAlpha(const State& state, int64_t nI,
                                    double R_lower, int64_t max_depth_sim,
                                    SimInterface* pomdp) {
  const std::optional<double> val = GetNode(nI).GetAlpha(state);
  if (val.has_value()) return val.value();
  const double V = SimulateTrajectory(nI, state, max_depth_sim, R_lower, pomdp);
  GetNode(nI).SetAlpha(state, V);
  return V;
}

AlphaVectorFSC InitialiseFSC(const PathToTerminal& ptt,
                             const BeliefDistribution& initial_belief,
                             int64_t max_depth, int64_t max_node_size,
                             SimInterface* pomdp) {
  for (const auto& [s, p] : initial_belief) (void)ptt.path(s, max_depth);

  const auto path_tree = ptt.buildPathTree();

  AlphaVectorFSC fsc(max_node_size);
  std::unordered_map<int64_t, int64_t> node_map;
  for (const auto& [s, node] : path_tree) {
    auto curr_node = node;
    while (curr_node != nullptr && curr_node->action != -1) {
      if (!node_map.contains(curr_node->id)) {
        node_map[curr_node->id] =
            fsc.AddNode(AlphaVectorNode(curr_node->action));
        curr_node = curr_node->nextNode;
      } else {
        break;
      }
    }
  }

  std::unordered_map<int64_t, std::unordered_map<int64_t, int64_t>> edge_map;
  for (const auto& [s, node] : path_tree) {
    auto state = s;
    auto curr_node = node;
    while (curr_node != nullptr && curr_node->action != -1) {
      const auto [sNext, obs, reward, done] = pomdp->Step(state, node->action);
      edge_map[node_map[curr_node->id]][obs] = node_map[node->nextNode->id];
      curr_node = curr_node->nextNode;
      state = sNext;
    }
  }
  for (const auto& [nI, edges] : edge_map) fsc.UpdateEdge(nI, edges);

  return fsc;
}

}  // namespace MCVI
