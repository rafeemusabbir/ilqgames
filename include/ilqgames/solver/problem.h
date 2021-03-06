/*
 * Copyright (c) 2019, The Regents of the University of California (Regents).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *    3. Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Please contact the author(s) of this library if you have any questions.
 * Authors: David Fridovich-Keil   ( dfk@eecs.berkeley.edu )
 */

///////////////////////////////////////////////////////////////////////////////
//
// Base class specifying the problem interface for managing calls to the core
// ILQGame solver. Specific examples will be derived from this class.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef ILQGAMES_SOLVER_PROBLEM_H
#define ILQGAMES_SOLVER_PROBLEM_H

#include <ilqgames/solver/game_solver.h>
#include <ilqgames/utils/solver_log.h>
#include <ilqgames/utils/strategy.h>
#include <ilqgames/utils/types.h>

#include <limits>
#include <memory>
#include <vector>

namespace ilqgames {

class Problem {
 public:
  virtual ~Problem() {}

  // Solve this game. Returns log populated by solver.
  std::shared_ptr<SolverLog> Solve(
      Time max_runtime = std::numeric_limits<Time>::infinity());

  // Reset the initial time and change nothing else.
  void ResetInitialTime(Time t0) { operating_point_->t0 = t0; }
  void ResetInitialState(const VectorXf& x0) { x0_ = x0; }

  // Update initial state and modify previous strategies and operating
  // points to start at the specified runtime after the current time t0.
  // Since time is continuous and we will want to maintain the same fixed
  // discretization, we will integrate x0 forward from t0 by approximately
  // planner_runtime, then find the nearest state in the existing plan to that
  // state, and start from there. By default, extends operating points and
  // strategies as follows:
  // 1. new controls are zero
  // 2. new states are those that result from zero control
  // 3. new strategies are also zero
  virtual void SetUpNextRecedingHorizon(const VectorXf& x0, Time t0,
                                        Time planner_runtime = 0.1);

  // Overwrite existing solution with the given operating point and strategies.
  // Truncates to fit in the same memory.
  void OverwriteSolution(const OperatingPoint& operating_point,
                         const std::vector<Strategy>& strategies);

  // Accessors.
  const GameSolver& Solver() const { return *solver_; }
  const VectorXf& InitialState() const { return x0_; }
  const OperatingPoint& CurrentOperatingPoint() const {
    return *operating_point_;
  }
  const std::vector<Strategy>& CurrentStrategies() const {
    return *strategies_;
  }

 protected:
  Problem() {}

  // Create a new log. This may be overridden by derived classes (e.g., to
  // change the name of the log).
  virtual std::shared_ptr<SolverLog> CreateNewLog() const;

  // Solver.
  std::unique_ptr<GameSolver> solver_;

  // Initial condition.
  VectorXf x0_;

  // Converged strategies and operating points for all players.
  std::unique_ptr<OperatingPoint> operating_point_;
  std::unique_ptr<std::vector<Strategy>> strategies_;
};  // class Problem

}  // namespace ilqgames

#endif
