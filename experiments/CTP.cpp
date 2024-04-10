#include <iostream>
#include <random>

#include "CTP_graph.h"
#include "MCVI.h"
#include "SimInterface.h"
#include "statespace.h"

using namespace MCVI;

class CTP : public MCVI::SimInterface {
 private:
  mutable std::mt19937_64 rng;
  std::vector<int> nodes;
  std::unordered_map<std::pair<int, int>, double, pairhash>
      edges;  // bidirectional, smallest node is first, double is weight
  std::unordered_map<std::pair<int, int>, double, pairhash>
      stoch_edges;  // probability of being blocked
  int goal;
  int origin;
  StateSpace stateSpace;
  StateSpace observationSpace;
  std::vector<std::string> actions;
  std::vector<std::string> observations;
  double _move_reward = -1;
  double _idle_reward = -1;
  double _bad_action_reward = -50;

 public:
  CTP(uint64_t seed = std::random_device{}())
      : rng(seed),
        nodes(CTPNodes),
        edges(CTPEdges),
        stoch_edges(CTPStochEdges),
        goal(CTPGoal),
        origin(CTPOrigin),
        stateSpace(initStateSpace()),
        observationSpace(initObsSpace()),
        actions(initActions()),
        observations(initObs()) {}

  int GetSizeOfObs() const override { return observationSpace.size(); }
  int GetSizeOfA() const override { return actions.size(); }
  double GetDiscount() const override { return 0.95; }
  int GetNbAgent() const override { return 1; }
  const std::vector<std::string>& getActions() const { return actions; }
  const std::vector<std::string>& getObs() const { return observations; }

  std::tuple<int, int, double, bool> Step(int sI, int aI) override {
    int sNext;
    const double reward = applyActionToState(sI, aI, sNext);
    const int oI = observeState(sNext);
    const bool finished = stateSpace.getStateFactorElem(sNext, "loc") == goal;
    // sI_next, oI, Reward, Done
    return std::tuple<int, int, double, bool>(sNext, oI, reward, finished);
  }

  int SampleStartState() override {
    std::uniform_real_distribution<> unif(0, 1);
    std::map<std::string, int> state;
    // agent starts at origin
    state["loc"] = origin;
    // stochastic edge status
    for (const auto& [edge, p] : stoch_edges)
      state[edge2str(edge)] = (unif(rng)) < p ? 0 : 1;
    return stateSpace.stateIndex(state);
  }

 private:
  std::vector<std::string> initActions() const {
    std::vector<std::string> acts;
    for (const auto& n : nodes) acts.push_back(std::to_string(n));
    return acts;
  }

  std::string edge2str(std::pair<int, int> e) const {
    return "e" + std::to_string(e.first) + "_" + std::to_string(e.second);
  }

  StateSpace initStateSpace() const {
    std::map<std::string, std::vector<int>> state_factors;
    // agent location
    state_factors["loc"] = nodes;
    // stochastic edge status
    for (const auto& [edge, _] : stoch_edges)
      state_factors[edge2str(edge)] = {0, 1};  // 0 = blocked, 1 = unblocked
    return StateSpace(state_factors);
  }

  // agent can observe any element from state space or -1 (unknown)
  StateSpace initObsSpace() const {
    std::map<std::string, std::vector<int>> observation_factors;
    // agent location
    std::vector<int> agent_locs = nodes;
    agent_locs.push_back(-1);
    observation_factors["loc"] = agent_locs;
    // stochastic edge status
    for (const auto& [edge, _] : stoch_edges)
      observation_factors[edge2str(edge)] = {0, 1, -1};
    return StateSpace(observation_factors);
  }

  std::string map2string(const std::map<std::string, int>& map) const {
    std::stringstream ss;
    for (auto it = map.begin(); it != map.end(); ++it) {
      ss << it->first << ": " << it->second;
      if (std::next(it) != map.end()) {
        ss << ", ";
      }
    }
    return ss.str();
  }

  std::vector<std::string> initObs() const {
    std::vector<std::string> obs;  // Observation space can be very large!
    // for (size_t o = 0; o < observationSpace.size(); ++o)
    //   obs.push_back(map2string(observationSpace.at(o)));
    return obs;
  }

  bool nodesAdjacent(int a, int b, int state) const {
    if (a == b) return true;
    const auto edge = a < b ? std::pair(a, b) : std::pair(b, a);
    if (edges.find(edge) == edges.end()) return false;  // edge does not exist

    // check if edge is stochastic
    const auto stoch_ptr = stoch_edges.find(edge);
    if (stoch_ptr == stoch_edges.end()) return true;  // deterministic edge

    // check if edge is unblocked
    return stateSpace.getStateFactorElem(state, edge2str(edge)) ==
           1;  // traversable
  }

  double applyActionToState(int state, int action, int& sNext) const {
    sNext = state;
    const int loc = stateSpace.getStateFactorElem(state, "loc");
    if (!nodesAdjacent(loc, action, state)) return _bad_action_reward;

    sNext = stateSpace.updateStateFactor(state, "loc", action);
    if (loc == action) return _idle_reward;
    return action < loc ? -edges.at({action, loc}) : -edges.at({loc, action});
  }

  int observeState(int state) const {
    std::map<std::string, int> observation;

    const int loc = stateSpace.getStateFactorElem(state, "loc");
    observation["loc"] = loc;

    // stochastic edge status
    for (const auto& [edge, _] : stoch_edges) {
      if (loc == edge.first || loc == edge.second) {
        const int status = stateSpace.getStateFactorElem(state, edge2str(edge));
        observation[edge2str(edge)] = status;
      } else {
        observation[edge2str(edge)] = -1;
      }
    }
    return observationSpace.stateIndex(observation);
  }
};

int main() {
  // Initialise the POMDP
  std::cout << "Initialising CTP" << std::endl;
  auto pomdp = CTP();

  const int64_t nb_particles_b0 = 10000;
  const int64_t max_node_size = 10000;

  // Sample the initial belief
  std::cout << "Sampling initial belief" << std::endl;
  std::vector<int64_t> particles;
  for (int i = 0; i < nb_particles_b0; ++i)
    particles.push_back(pomdp.SampleStartState());
  const auto init_belief = BeliefParticles(particles);

  // Set the Q-learning policy
  const int64_t max_sim_depth = 15;
  const double learning_rate = 0.9;
  const int64_t nb_episode_size = 30;
  const int64_t nb_max_episode = 10;
  const int64_t nb_sim = 40;
  const double decay_Q_learning = 0.01;
  const double epsilon_Q_learning = 0.001;
  std::cout << "Learning bounds" << std::endl;
  const auto q_policy = QLearningPolicy(
      learning_rate, decay_Q_learning, max_sim_depth, nb_max_episode,
      nb_episode_size, nb_sim, epsilon_Q_learning);

  // Initialise the FSC
  std::vector<int64_t> action_space;
  std::vector<int64_t> observation_space;
  std::cout << "Initialising FSC" << std::endl;
  for (int i = 0; i < pomdp.GetSizeOfA(); ++i) action_space.push_back(i);
  for (int i = 0; i < pomdp.GetSizeOfObs(); ++i) observation_space.push_back(i);
  const auto init_fsc =
      AlphaVectorFSC(max_node_size, action_space, observation_space);

  // Run MCVI
  std::cout << "Running MCVI" << std::endl;
  auto planner = MCVIPlanner(&pomdp, init_fsc, init_belief, q_policy);
  const int64_t nb_sample = 1000;
  const double converge_thresh = 0.1;
  const int64_t max_iter = 30;
  const auto fsc =
      planner.Plan(max_sim_depth, nb_sample, converge_thresh, max_iter);

  //   fsc.GenerateGraphviz(std::cerr, pomdp.getActions(), pomdp.getObs());

  // Simulate the resultant FSC
  planner.SimulationWithFSC(20);

  return 0;
}
