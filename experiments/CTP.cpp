#include "CTP.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>

#include "AOStar.h"
#include "MCVI.h"

using namespace MCVI;

static double s_time_diff(const std::chrono::steady_clock::time_point& begin,
                          const std::chrono::steady_clock::time_point& end) {
  return (std::chrono::duration_cast<std::chrono::milliseconds>(end - begin)
              .count()) /
         1000.0;
}

void runMCVI(CTP* pomdp, const BeliefDistribution& init_belief,
             std::mt19937_64& rng, int64_t max_sim_depth, int64_t max_node_size,
             int64_t eval_depth, int64_t eval_epsilon, double converge_thresh,
             int64_t max_iter, int64_t max_computation_ms,
             int64_t max_eval_steps, int64_t n_eval_trials,
             int64_t nb_particles_b0) {
  // Initialise heuristic
  PathToTerminal ptt(pomdp);

  // Initialise the FSC
  std::cout << "Initialising FSC" << std::endl;
  const auto init_fsc = AlphaVectorFSC(max_node_size);
  //   const auto init_fsc =
  //       InitialiseFSC(ptt, init_belief, max_sim_depth, max_node_size,
  //       &pomdp);
  //   init_fsc.GenerateGraphviz(std::cerr, pomdp.getActions(), pomdp.getObs());

  // Run MCVI
  std::cout << "Running MCVI" << std::endl;
  const std::chrono::steady_clock::time_point mcvi_begin =
      std::chrono::steady_clock::now();
  auto planner = MCVIPlanner(pomdp, init_fsc, init_belief, ptt, rng);
  const auto [fsc, root] =
      planner.Plan(max_sim_depth, converge_thresh, max_iter, max_computation_ms,
                   eval_depth, eval_epsilon);
  const std::chrono::steady_clock::time_point mcvi_end =
      std::chrono::steady_clock::now();
  std::cout << "MCVI complete (" << s_time_diff(mcvi_begin, mcvi_end)
            << " seconds)" << std::endl;

  // Draw FSC plot
  std::fstream fsc_graph("fsc.dot", std::fstream::out);
  fsc.GenerateGraphviz(fsc_graph, pomdp->getActions(), pomdp->getObs());
  fsc_graph.close();

  // Simulate the resultant FSC
  std::cout << "Simulation with up to " << max_eval_steps
            << " steps:" << std::endl;
  planner.SimulationWithFSC(max_eval_steps);
  std::cout << std::endl;

  // Evaluate the FSC policy
  std::cout << "Evaluation of policy (" << max_eval_steps << " steps, "
            << n_eval_trials << " trials):" << std::endl;
  planner.EvaluationWithSimulationFSC(max_eval_steps, n_eval_trials,
                                      nb_particles_b0);
  std::cout << "detMCVI policy FSC contains " << fsc.NumNodes() << " nodes."
            << std::endl;
  std::cout << std::endl;

  // Draw the internal belief tree
  std::fstream belief_tree("belief_tree.dot", std::fstream::out);
  root->DrawBeliefTree(belief_tree);
  belief_tree.close();
}

void runAOStar(CTP* pomdp, const BeliefDistribution& init_belief,
               std::mt19937_64& rng, int64_t eval_depth, int64_t eval_epsilon,
               int64_t max_iter, int64_t max_computation_ms,
               int64_t max_eval_steps, int64_t n_eval_trials,
               int64_t nb_particles_b0) {
  // Initialise heuristic
  PathToTerminal ptt(pomdp);

  // Create root belief node
  std::shared_ptr<BeliefTreeNode> root = CreateBeliefTreeNode(
      init_belief, 0, ptt, eval_depth, eval_epsilon, pomdp);

  // Run AO*
  std::cout << "Running AO* on belief tree" << std::endl;
  const std::chrono::steady_clock::time_point ao_begin =
      std::chrono::steady_clock::now();
  RunAOStar(root, max_iter, max_computation_ms, ptt, eval_depth, eval_epsilon,
            pomdp);
  const std::chrono::steady_clock::time_point ao_end =
      std::chrono::steady_clock::now();
  std::cout << "AO* complete (" << s_time_diff(ao_begin, ao_end) << " seconds)"
            << std::endl;

  // Draw policy tree
  std::fstream policy_tree("greedy_policy_tree.dot", std::fstream::out);
  const int64_t n_greedy_nodes = root->DrawPolicyTree(policy_tree);
  policy_tree.close();

  // Evaluate policy
  std::cout << "Evaluation of alternative (AO* greedy) policy ("
            << max_eval_steps << " steps, " << n_eval_trials
            << " trials):" << std::endl;
  EvaluationWithGreedyTreePolicy(root, max_eval_steps, n_eval_trials,
                                 nb_particles_b0, pomdp, rng, ptt, "AO*");
  std::cout << "AO* greedy policy tree contains " << n_greedy_nodes << " nodes."
            << std::endl;
}

void parseCommandLine(int argc, char* argv[], int64_t& runtime_ms) {
  if (argc > 1) {
    for (int i = 1; i < argc; ++i) {
      if (std::string(argv[i]) == "--runtime" && i + 1 < argc) {
        runtime_ms = std::stoi(argv[i + 1]);
        break;
      }
    }
  }
}

int main(int argc, char* argv[]) {
  std::mt19937_64 rng(std::random_device{}());

  // Initialise the POMDP
  std::cout << "Initialising CTP" << std::endl;
  auto pomdp = CTP(rng);

  std::cout << "Observation space size: " << pomdp.GetSizeOfObs() << std::endl;

  std::fstream ctp_graph("ctp_graph.dot", std::fstream::out);
  pomdp.visualiseGraph(ctp_graph);
  ctp_graph.close();

  // Initial belief parameters
  const int64_t nb_particles_b0 = 100000;
  const int64_t max_belief_samples = 20000;

  // MCVI parameters
  const int64_t max_sim_depth = 30;
  const int64_t max_node_size = 10000;
  const int64_t eval_depth = 30;
  const int64_t eval_epsilon = 0.005;
  const double converge_thresh = 0.005;
  const int64_t max_iter = 500;
  int64_t max_time_ms = 10000;

  // Evaluation parameters
  const int64_t max_eval_steps = 30;
  const int64_t n_eval_trials = 10000;

  parseCommandLine(argc, argv, max_time_ms);

  // Sample the initial belief
  std::cout << "Sampling initial belief" << std::endl;
  auto init_belief = SampleInitialBelief(nb_particles_b0, &pomdp);
  std::cout << "Initial belief size: " << init_belief.size() << std::endl;
  if (max_belief_samples < init_belief.size()) {
    std::cout << "Downsampling belief" << std::endl;
    init_belief = DownsampleBelief(init_belief, max_belief_samples, rng);
  }

  // Run MCVI
  auto mcvi_ctp = new CTP(pomdp);
  runMCVI(mcvi_ctp, init_belief, rng, max_sim_depth, max_node_size, eval_depth,
          eval_epsilon, converge_thresh, max_iter, max_time_ms, max_eval_steps,
          n_eval_trials, 10 * nb_particles_b0);

  // Compare to AO*
  auto aostar_ctp = new CTP(pomdp);
  runAOStar(aostar_ctp, init_belief, rng, eval_depth, eval_epsilon, max_iter,
            max_time_ms, max_eval_steps, n_eval_trials, 10 * nb_particles_b0);

  return 0;
}
