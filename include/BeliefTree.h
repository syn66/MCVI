/* This file has been written and/or modified by the following people:
 *
 * Yang You
 * Alex Schutz
 *
 */

#pragma once

#include <memory>
#include <unordered_map>

#include "BeliefDistribution.h"
#include "Bound.h"

namespace MCVI {

class BeliefTreeNode {
 private:
  using BeliefEdgeMap = std::unordered_map<
      int64_t, std::unordered_map<int64_t, std::shared_ptr<BeliefTreeNode>>>;

  BeliefDistribution _belief;
  BeliefEdgeMap _child_nodes;
  int64_t _best_action;
  std::unordered_map<int64_t, double>
      _R_a;  // a map from actions to expected instant reward
  std::unordered_map<int64_t, std::unordered_map<int64_t, double>>
      _a_o_weights;  // a map that stores the weights of different observations
                     // for a given action
  double _upper_bound;
  double _lower_bound;
  int64_t _fsc_node_index;

 public:
  BeliefTreeNode(const BeliefDistribution& belief, int64_t best_action,
                 double upper_bound, double lower_bound)
      : _belief(belief),
        _best_action(best_action),
        _upper_bound(upper_bound),
        _lower_bound(lower_bound),
        _fsc_node_index(-1) {}

  void AddChild(int64_t action, int64_t observation,
                std::shared_ptr<BeliefTreeNode> child);

  const BeliefDistribution& GetBelief() const { return _belief; }

  int64_t GetFSCNodeIndex() const { return _fsc_node_index; }
  void SetFSCNodeIndex(int64_t idx) { _fsc_node_index = idx; }

  int64_t GetBestAction() const { return _best_action; }
  void SetBestAction(int64_t action, double lower_bound);

  double GetUpper() const { return _upper_bound; }
  void SetUpper(double upper_bound) { _upper_bound = upper_bound; }
  double GetLower() const { return _lower_bound; }

  bool HasReward(int64_t action) const { return _R_a.contains(action); }
  double GetReward(int64_t action) const { return _R_a.at(action); }
  void SetReward(int64_t action, double reward) { _R_a[action] = reward; }

  const std::unordered_map<int64_t, double>& GetWeights(int64_t action) const {
    return _a_o_weights.at(action);
  }
  void SetWeight(int64_t action, int64_t observation, double weight) {
    auto& aw = _a_o_weights[action];
    aw[observation] = weight;
  }

  std::shared_ptr<BeliefTreeNode> GetChild(int64_t action, int64_t observation);
};

/// @brief Add a child belief to the parent given an action and observation edge
void CreateBeliefTreeNode(std::shared_ptr<BeliefTreeNode> parent,
                          int64_t action, int64_t observation,
                          const BeliefDistribution& belief, int64_t num_actions,
                          const PathToTerminal& heuristic, int64_t eval_depth,
                          double eval_epsilon, SimInterface* sim);

std::shared_ptr<BeliefTreeNode> CreateBeliefRootNode(
    const BeliefDistribution& belief, int64_t num_actions,
    const PathToTerminal& heuristic, int64_t eval_depth, double eval_epsilon,
    SimInterface* sim);

/// @brief Sample beliefs from a belief tree with heuristics
void SampleBeliefs(
    std::shared_ptr<BeliefTreeNode> node, int64_t state, int64_t depth,
    int64_t max_depth, SimInterface* pomdp, const PathToTerminal& heuristic,
    int64_t eval_depth, double eval_epsilon,
    std::vector<std::shared_ptr<BeliefTreeNode>>& traversal_list);

/// @brief Generate a set of next beliefs mapped by observation, obtained by
/// taking `action` in belief node `node`. Return the most probable observation
/// and the next beliefs
std::pair<int64_t, std::unordered_map<int64_t, BeliefDistribution>>
BeliefUpdate(std::shared_ptr<BeliefTreeNode> node, int64_t action,
             SimInterface* pomdp);

// Update the upper bound of the given node based on its children
double UpdateUpperBound(std::shared_ptr<BeliefTreeNode> node, double gamma,
                        int64_t depth);

}  // namespace MCVI
