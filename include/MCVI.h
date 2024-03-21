/* This file has been written and/or modified by the following people:
 *
 * Yang You
 * Alex Schutz
 *
 */

#ifndef _MCVIPLANNER_H_
#define _MCVIPLANNER_H_

#include <iostream>

#include "SimInterface.h"

class MCVI {
 private:
  /* data */
 public:
  MCVI(/* args */);
  ~MCVI();
};

MCVI::MCVI(/* args */) {}

MCVI::~MCVI() {}

/// @brief Determine the lower bound reward pf belief, by choosing an action
/// that maximises the minimum instant reward for all situations
double FindRLower(SimInterface* pomdp, const BeliefParticles& b0,
                  const std::vector<int>& action_space, int64_t max_restarts,
                  double epsilon, int64_t max_depth);

#endif /* !_MCVIPLANNER_H_ */
