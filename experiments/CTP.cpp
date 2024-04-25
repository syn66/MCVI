#include <iostream>
#include <random>

#include "CTP_graph.h"
#include "MCVI.h"
#include "SimInterface.h"
#include "statespace.h"

using namespace MCVI;

class CTP : public MCVI::SimInterface {
 private:
  std::mt19937_64& rng;
  std::vector<int64_t> nodes;
  std::unordered_map<std::pair<int64_t, int64_t>, double, pairhash>
      edges;  // bidirectional, smallest node is first, double is weight
  std::unordered_map<std::pair<int64_t, int64_t>, double, pairhash>
      stoch_edges;  // probability of being blocked
  int64_t goal;
  int64_t origin;
  StateSpace stateSpace;
  StateSpace observationSpace;
  std::vector<std::string> actions;
  std::vector<std::string> observations;
  double _idle_reward = -5;
  double _bad_action_reward = -50;

 public:
  CTP(std::mt19937_64& rng)
      : rng(rng),
        nodes(CTPNodes),
        edges(CTPEdges),
        stoch_edges(CTPStochEdges),
        goal(CTPGoal),
        origin(CTPOrigin),
        stateSpace(initStateSpace()),
        observationSpace(initObsSpace()),
        actions(initActions()),
        observations(initObs()) {}

  int64_t GetSizeOfObs() const override { return observationSpace.size(); }
  int64_t GetSizeOfA() const override { return actions.size(); }
  double GetDiscount() const override { return 0.95; }
  int64_t GetNbAgent() const override { return 1; }
  const std::vector<std::string>& getActions() const { return actions; }
  const std::vector<std::string>& getObs() const { return observations; }
  int64_t getGoal() const { return goal; }
  bool IsTerminal(int64_t sI) const override {
    return stateSpace.getStateFactorElem(sI, "loc") == goal;
  }

  std::tuple<int64_t, int64_t, double, bool> Step(int64_t sI,
                                                  int64_t aI) override {
    int64_t sNext;
    const double reward = applyActionToState(sI, aI, sNext);
    const int64_t oI = observeState(sNext);
    const bool finished = stateSpace.getStateFactorElem(sNext, "loc") == goal;
    // sI_next, oI, Reward, Done
    return std::tuple<int64_t, int64_t, double, bool>(sNext, oI, reward,
                                                      finished);
  }

  int64_t SampleStartState() override {
    std::uniform_real_distribution<> unif(0, 1);
    std::map<std::string, int64_t> state;
    // agent starts at origin
    state["loc"] = origin;
    // stochastic edge status
    for (const auto& [edge, p] : stoch_edges)
      state[edge2str(edge)] = (unif(rng)) < p ? 0 : 1;
    return stateSpace.stateIndex(state);
  }

  void visualiseGraph(std::ostream& os) {
    if (!os) throw std::logic_error("Unable to write graph to file");

    os << "graph G {" << std::endl;

    for (const auto& i : nodes) {
      os << "  " << i << " [label=\"" << i << "\"";
      if (i == origin) os << ", fillcolor=\"#ff7f0e\", style=filled";
      if (i == goal) os << ", fillcolor=\"#2ca02c\", style=filled";
      os << "];" << std::endl;
    }

    for (const auto& [edge, weight] : edges) {
      auto stochEdge = stoch_edges.find(edge);
      if (stochEdge != stoch_edges.end()) {
        os << "  " << edge.first << " -- " << edge.second << " [label=\""
           << stochEdge->second << " : " << weight << "\", style=dashed];"
           << std::endl;
      } else {
        os << "  " << edge.first << " -- " << edge.second << " [label=\""
           << weight << "\"];" << std::endl;
      }
    }

    os << "}" << std::endl;
  }

 private:
  std::vector<std::string> initActions() const {
    std::vector<std::string> acts;
    for (const auto& n : nodes) acts.push_back(std::to_string(n));
    return acts;
  }

  std::string edge2str(std::pair<int64_t, int64_t> e) const {
    return "e" + std::to_string(e.first) + "_" + std::to_string(e.second);
  }

  StateSpace initStateSpace() const {
    std::map<std::string, std::vector<int64_t>> state_factors;
    // agent location
    state_factors["loc"] = nodes;
    // stochastic edge status
    for (const auto& [edge, _] : stoch_edges)
      state_factors[edge2str(edge)] = {0, 1};  // 0 = blocked, 1 = unblocked
    StateSpace ss(state_factors);
    std::cout << "State space size: " << ss.size() << std::endl;
    return ss;
  }

  // agent can observe any element from state space or -1 (unknown)
  StateSpace initObsSpace() const {
    std::map<std::string, std::vector<int64_t>> observation_factors;
    // agent location
    std::vector<int64_t> agent_locs = nodes;
    agent_locs.push_back(-1);
    observation_factors["loc"] = agent_locs;
    // stochastic edge status
    for (const auto& [edge, _] : stoch_edges)
      observation_factors[edge2str(edge)] = {0, 1, -1};
    StateSpace os(observation_factors);
    size_t max_size = (size_t)-1;
    std::cout << "Max size: " << max_size << std::endl;
    std::cout << "Observation space size: " << os.size() << std::endl;
    return os;
  }

  std::string map2string(const std::map<std::string, int64_t>& map) const {
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

  bool nodesAdjacent(int64_t a, int64_t b, int64_t state) const {
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

  double applyActionToState(int64_t state, int64_t action,
                            int64_t& sNext) const {
    sNext = state;
    const int64_t loc = stateSpace.getStateFactorElem(state, "loc");
    if (!nodesAdjacent(loc, action, state)) return _bad_action_reward;

    sNext = stateSpace.updateStateFactor(state, "loc", action);
    if (loc == action) {
      if (loc == goal) return 0;
      return _idle_reward;
    }
    return action < loc ? -edges.at({action, loc}) : -edges.at({loc, action});
  }

  int64_t observeState(int64_t state) const {
    std::map<std::string, int64_t> observation;

    const int64_t loc = stateSpace.getStateFactorElem(state, "loc");
    observation["loc"] = loc;

    // stochastic edge status
    for (const auto& [edge, _] : stoch_edges) {
      if (loc == edge.first || loc == edge.second) {
        const int64_t status =
            stateSpace.getStateFactorElem(state, edge2str(edge));
        observation[edge2str(edge)] = status;
      } else {
        observation[edge2str(edge)] = -1;
      }
    }
    return observationSpace.stateIndex(observation);
  }
};

int main() {
  std::mt19937_64 rng(std::random_device{}());

  // Initialise the POMDP
  std::cout << "Initialising CTP" << std::endl;
  auto pomdp = CTP(rng);

  pomdp.visualiseGraph(std::cerr);

  const int64_t nb_particles_b0 = 100000;
  const int64_t max_node_size = 10000;
  const int64_t max_sim_depth = 15;
  const int64_t max_belief_samples = 10000;

  // Sample the initial belief
  std::cout << "Sampling initial belief" << std::endl;
  std::unordered_map<int64_t, int64_t> state_counts;
  for (int64_t i = 0; i < nb_particles_b0; ++i)
    state_counts[pomdp.SampleStartState()] += 1;
  auto init_belief = BeliefDistribution();
  for (const auto& [state, count] : state_counts)
    init_belief[state] = (double)count / nb_particles_b0;
  std::cout << "Initial belief size: " << init_belief.size() << std::endl;
  if (max_belief_samples < init_belief.size()) {
    std::cout << "Downsampling belief" << std::endl;
    const auto shuffled_init =
        weightedShuffle(init_belief, rng, max_belief_samples);
    double prob_sum = 0.0;
    for (const auto& [state, prob] : shuffled_init) prob_sum += prob;
    init_belief.clear();
    for (const auto& [state, prob] : shuffled_init)
      init_belief[state] = prob / prob_sum;
  }

  // Initialise the FSC
  std::cout << "Initialising FSC" << std::endl;
  PathToTerminal ptt(&pomdp);
  const auto init_fsc = AlphaVectorFSC(max_node_size);
  //   const auto init_fsc =
  //       InitialiseFSC(ptt, init_belief, max_sim_depth, max_node_size,
  //       &pomdp);
  //   init_fsc.GenerateGraphviz(std::cerr, pomdp.getActions(), pomdp.getObs());

  // Run MCVI
  std::cout << "Running MCVI" << std::endl;
  const int64_t eval_depth = 20;
  const int64_t eval_epsilon = 0.01;
  auto planner = MCVIPlanner(&pomdp, init_fsc, init_belief, ptt, rng);
  const double converge_thresh = 0.01;
  const int64_t max_iter = 30;
  const auto fsc = planner.Plan(max_sim_depth, converge_thresh, max_iter,
                                eval_depth, eval_epsilon);

  fsc.GenerateGraphviz(std::cerr, pomdp.getActions(), pomdp.getObs());

  // Simulate the resultant FSC
  planner.SimulationWithFSC(15);

  planner.EvaluationWithSimulationFSC(15, 1000);

  return 0;
}
