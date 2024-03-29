
#include "../include/MCVI.h"

#include <algorithm>
#include <limits>

static bool CmpPair(const std::pair<int64_t, double>& p1,
                    const std::pair<int64_t, double>& p2) {
  return p1.second < p2.second;
}

double SimulateTrajectory(int64_t nI, AlphaVectorFSC& fsc, int64_t state,
                          int64_t max_depth, SimInterface* pomdp) {
  const double gamma = pomdp->GetDiscount();
  double V_n_s = 0.0;
  int64_t nI_current = nI;
  for (int64_t step = 0; step < max_depth; ++step) {
    const int64_t action = (nI_current != -1)
                               ? fsc.GetNode(nI_current).GetBestAction()
                               : pomdp->RandomAction();
    const auto [sNext, obs, reward, done] = pomdp->Step(state, action);
    if (nI_current != -1) nI_current = fsc.GetEtaValue(nI_current, action, obs);

    V_n_s += std::pow(gamma, step) * reward;
    if (done) break;
    state = sNext;
  }

  return V_n_s;
}

std::pair<double, int64_t> FindMaxValueNode(const AlphaVectorNode& node,
                                            int64_t a, int64_t o) {
  const auto& v = node.GetActionObservationValues(a, o);
  const auto it = std::max_element(std::begin(v), std::end(v), CmpPair);
  return {it->second, it->first};
}

int64_t InsertNode(const AlphaVectorNode& node,
                   const AlphaVectorFSC::EdgeMap& edges, AlphaVectorFSC& fsc) {
  const int64_t nI = fsc.AddNode(node);
  fsc.UpdateEta(nI, edges);
  return nI;
}

int64_t FindOrInsertNode(const AlphaVectorNode& node,
                         const AlphaVectorFSC::EdgeMap& edges,
                         const std::vector<int64_t>& observation_space,
                         AlphaVectorFSC& fsc) {
  const int64_t action = node.GetBestAction();
  for (int64_t nI = 0; nI < fsc.NumNodes(); ++nI) {
    // First check the best action
    if (fsc.GetNode(nI).GetBestAction() == action) {
      for (const auto& obs : observation_space) {
        const int64_t edge_node = fsc.GetEtaValue(nI, action, obs);
        if (edge_node == -1 || edge_node != edges.at({action, obs}))
          return InsertNode(node, edges, fsc);
      }
      return nI;
    }
  }
  return InsertNode(node, edges, fsc);
}

void BackUp(BeliefTreeNode& Tr_node, AlphaVectorFSC& fsc, int64_t max_depth_sim,
            int64_t nb_sample, SimInterface* pomdp,
            const std::vector<int64_t>& action_space,
            const std::vector<int64_t>& observation_space) {
  const double gamma = pomdp->GetDiscount();
  const BeliefParticles& belief = Tr_node.GetParticles();
  auto node_new = AlphaVectorNode(action_space, observation_space);

  AlphaVectorFSC::EdgeMap node_edges;
  for (const auto& action : action_space) {
    for (int64_t i = 0; i < nb_sample; ++i) {
      const int64_t state = belief.SampleOneState();
      const auto [sNext, obs, reward, done] = pomdp->Step(state, action);
      node_new.AddR(action, reward);
      for (int64_t nI = 0; (size_t)nI < fsc.NumNodes(); ++nI) {
        const double V_nI_sNext =
            SimulateTrajectory(nI, fsc, sNext, max_depth_sim, pomdp);
        node_new.UpdateValue(action, obs, nI, V_nI_sNext);
      }
    }

    for (const auto& obs : observation_space) {
      const auto [V_a_o, nI_a_o] = FindMaxValueNode(node_new, action, obs);
      node_edges[{action, obs}] = nI_a_o;
      node_new.AddQ(action, gamma * V_a_o);
    }
    node_new.AddQ(action, node_new.GetR(action));
    node_new.NormaliseQ(action, nb_sample);
  }

  node_new.UpdateBestValue(Tr_node);
  const int64_t nI =
      FindOrInsertNode(node_new, node_edges, observation_space, fsc);
  Tr_node.SetFSCNodeIndex(nI);
}

/*
void MCVIPlanning(int64_t nb_particles, const BeliefParticles& b0,
                  AlphaVectorFSC fsc, SimInterface* pomdp, double epsilon) {
  int64_t n_start = fsc.AddNode(b0);

  while (true) {
    // TODO: C++ implementation of the solver
  }
}
*/
