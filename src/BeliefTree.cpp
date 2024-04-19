#include "BeliefTree.h"

#include <limits>

namespace MCVI {

ActionNode::ActionNode(int64_t action, const BeliefDistribution& belief,
                       int64_t max_belief_samples,
                       const PathToTerminal& heuristic, int64_t eval_depth,
                       double eval_epsilon, SimInterface* pomdp)
    : _action(action) {
  BeliefUpdate(belief, max_belief_samples, heuristic, eval_depth, eval_epsilon,
               pomdp);
  CalculateBounds();
}

void ActionNode::BeliefUpdate(const BeliefDistribution& belief,
                              int64_t max_belief_samples,
                              const PathToTerminal& heuristic,
                              int64_t eval_depth, double eval_epsilon,
                              SimInterface* pomdp) {
  std::unordered_map<int64_t, BeliefDistribution> next_beliefs;

  auto belief_pdf = belief;
  double prob_sum = 0.0;
  for (int64_t sample = 0; sample < max_belief_samples; ++sample) {
    const auto [state, prob] = SamplePDFDestructive(belief_pdf);
    if (state == -1) break;  // Sampled all states in belief
    prob_sum += prob;
    auto [sNext, obs, reward, done] = pomdp->Step(state, GetAction());
    auto& obs_belief = next_beliefs[obs];
    obs_belief[sNext] += prob;
  }

  // Set weight based on likelihood of observations
  std::unordered_map<int64_t, BeliefDistribution> belief_map;
  for (const auto& [o, b] : next_beliefs) {
    double w = 0.0;
    for (const auto& [s, p] : b) w += p;
    w /= prob_sum;
    _o_weights[o] = w;
    // Renormalise next probabilities
    auto& belief = belief_map[o];
    for (const auto& [s, p] : b) belief[s] = p / w;
  }

  for (const auto& [o, b] : belief_map) {
    _observation_edges[o] =
        CreateBeliefTreeNode(b, heuristic, eval_depth, eval_epsilon, pomdp);
  }
}

void ActionNode::CalculateBounds() {
  double lower = 0;
  double upper = 0;

  for (const auto& [obs, child] : _observation_edges) {
    lower += child->GetLower() * _o_weights.at(obs);
    upper += child->GetUpper() * _o_weights.at(obs);
  }
  _avgLower = lower;
  _avgUpper = upper;
}

std::shared_ptr<BeliefTreeNode> ActionNode::GetChild(
    int64_t observation) const {
  auto it = _observation_edges.find(observation);
  if (it == _observation_edges.end()) return nullptr;
  return it->second;
}

std::shared_ptr<BeliefTreeNode> ActionNode::ChooseObservation(
    double target) const {
  double best_gap = -std::numeric_limits<double>::infinity();
  int64_t best_obs = -1;
  for (const auto& [obs, belief_node] : _observation_edges) {
    const double diff =
        (belief_node->GetUpper() - belief_node->GetLower()) - target;
    const double gap = diff * _o_weights.at(obs);
    std::cerr << "Obs: " << obs << " Target: " << target << " Diff: " << diff
              << " Gap: " << gap << " Weight: " << _o_weights.at(obs)
              << " Best gap: " << best_gap << " Best obs: " << best_obs
              << std::endl;
    if (gap > best_gap) {
      best_gap = gap;
      best_obs = obs;
    }
  }
  if (best_obs == -1) throw std::logic_error("Failed to find best observation");
  return _observation_edges.at(best_obs);
}

void BeliefTreeNode::AddChild(int64_t action, int64_t max_belief_samples,
                              const PathToTerminal& heuristic,
                              int64_t eval_depth, double eval_epsilon,
                              SimInterface* pomdp) {
  _action_edges.insert(
      {action, ActionNode(action, GetBelief(), max_belief_samples, heuristic,
                          eval_depth, eval_epsilon, pomdp)});
}

void BeliefTreeNode::SetBestAction(int64_t action, double lower_bound) {
  _best_action = action;
  _lower_bound = lower_bound;
}

std::shared_ptr<BeliefTreeNode> BeliefTreeNode::GetChild(
    int64_t action, int64_t observation) const {
  auto it = _action_edges.find(action);
  if (it == _action_edges.cend()) return nullptr;
  return it->second.GetChild(observation);
}

std::unordered_map<int64_t, std::shared_ptr<BeliefTreeNode>>
BeliefTreeNode::GetChildren(int64_t action) const {
  auto it = _action_edges.find(action);
  if (it == _action_edges.cend()) return {};
  return it->second.GetChildren();
}

std::shared_ptr<BeliefTreeNode> BeliefTreeNode::ChooseObservation(
    double target, int64_t max_belief_samples, const PathToTerminal& heuristic,
    int64_t eval_depth, double eval_epsilon, SimInterface* pomdp) {
  const int64_t action = GetBestAction();
  auto it = _action_edges.find(action);
  if (it == _action_edges.cend()) {
    AddChild(action, max_belief_samples, heuristic, eval_depth, eval_epsilon,
             pomdp);
  }

  return _action_edges.at(action).ChooseObservation(target);
}

std::shared_ptr<BeliefTreeNode> CreateBeliefTreeNode(
    const BeliefDistribution& belief, const PathToTerminal& heuristic,
    int64_t eval_depth, double eval_epsilon, SimInterface* sim) {
  const auto [a_best, U] = UpperBoundEvaluation(belief, heuristic, eval_depth);
  const auto root = std::make_shared<BeliefTreeNode>(
      belief, a_best, U,
      FindRLower(sim, belief, sim->GetSizeOfA(), eval_epsilon, eval_depth));
  return root;
}

}  // namespace MCVI
