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
// Three player intersection example. Ordering is given by the following:
// (P1, P2, P3) = (Car 1, Car 2, Pedestrian).
//
///////////////////////////////////////////////////////////////////////////////

#include "three_player_intersection_example.h"
#include <ilqgames/cost/curvature_cost.h>
#include <ilqgames/cost/nominal_path_length_cost.h>
#include <ilqgames/cost/quadratic_cost.h>
#include <ilqgames/cost/quadratic_polyline2_cost.h>
#include <ilqgames/cost/semiquadratic_cost.h>
#include <ilqgames/cost/semiquadratic_polyline2_cost.h>
#include <ilqgames/dynamics/concatenated_dynamical_system.h>
#include <ilqgames/dynamics/single_player_car_7d.h>
#include <ilqgames/dynamics/single_player_unicycle_5d.h>
#include <ilqgames/geometry/polyline2.h>
#include <ilqgames/solver/ilq_solver.h>
#include <ilqgames/solver/linesearching_ilq_solver.h>
#include <ilqgames/solver/problem.h>
#include <ilqgames/utils/log.h>
#include <ilqgames/utils/strategy.h>
#include <ilqgames/utils/types.h>

#include <math.h>
#include <memory>
#include <vector>

namespace ilqgames {

namespace {
// Time.
static constexpr Time kTimeStep = 0.1;     // s
static constexpr Time kTimeHorizon = 5.0;  // s
static constexpr size_t kNumTimeSteps =
    static_cast<size_t>(kTimeHorizon / kTimeStep);

// Car inter-axle distance.
static constexpr float kInterAxleLength = 4.0;  // m

// Cost weights.
static constexpr float kLaneCostWeight = 50.0;
static constexpr float kLaneBoundaryCostWeight = 200.0;
static constexpr float kACostWeight = 1.0;
static constexpr float kOmegaCostWeight = 50.0;
static constexpr float kSCostWeight = 0.0;
static constexpr float kMaxVCostWeight = 100.0;
static constexpr float kNominalVCostWeight = 0.0;
static constexpr float kCurvatureCostWeight = 0.0;
static constexpr float kGoalCostWeight = 10.0;

static constexpr bool kOrientedRight = true;

// Lane dimension.
static constexpr float kLaneHalfWidth = 4.0;  // m

// Goal points.
static constexpr float kP1GoalX = -6.0;  // m
static constexpr float kP1GoalY = 50.0;  // m

static constexpr float kP2GoalX = 50.0;  // m
static constexpr float kP2GoalY = 12.0;  // m

static constexpr float kP3GoalX = 5.0;   // m
static constexpr float kP3GoalY = 14.0;  // m

// Nominal and max speed.
static constexpr float kP1MaxV = 15.0;  // m/s
static constexpr float kP2MaxV = 15.0;  // m/s
static constexpr float kP3MaxV = 3.0;   // m/s
static constexpr float kMinV = 0.1;     // m/s

static constexpr float kP1NominalV = 10.0;  // m/s
static constexpr float kP2NominalV = 10.0;  // m/s
static constexpr float kP3NominalV = 1.0;   // m/s

// Initial state.
static constexpr float kP1InitialX = -5.0;   // m
static constexpr float kP2InitialX = -10.0;  // m
static constexpr float kP3InitialX = -12.0;  // m

static constexpr float kP1InitialY = -30.0;  // m
static constexpr float kP2InitialY = 30.0;   // m
static constexpr float kP3InitialY = 15.0;   // m

static constexpr float kP1InitialHeading = M_PI_2;   // rad
static constexpr float kP2InitialHeading = -M_PI_2;  // rad
static constexpr float kP3InitialHeading = 0.0;      // rad

static constexpr float kP1InitialSpeed = 5.0;  // m/s
static constexpr float kP2InitialSpeed = 5.0;  // m/s
static constexpr float kP3InitialSpeed = 1.0;  // m/s

// State dimensions.
using P1 = SinglePlayerCar7D;
using P2 = SinglePlayerCar7D;
using P3 = SinglePlayerUnicycle5D;

static constexpr Dimension kP1XIdx = P1::kPxIdx;
static constexpr Dimension kP1YIdx = P1::kPyIdx;
static constexpr Dimension kP1HeadingIdx = P1::kThetaIdx;
static constexpr Dimension kP1VIdx = P1::kVIdx;
static constexpr Dimension kP1KappaIdx = P1::kKappaIdx;
static constexpr Dimension kP1SIdx = P1::kSIdx;
static constexpr Dimension kP2XIdx = P1::kNumXDims + P2::kPxIdx;
static constexpr Dimension kP2YIdx = P1::kNumXDims + P2::kPyIdx;
static constexpr Dimension kP2HeadingIdx = P1::kNumXDims + P2::kThetaIdx;
static constexpr Dimension kP2VIdx = P1::kNumXDims + P2::kVIdx;
static constexpr Dimension kP2KappaIdx = P1::kNumXDims + P2::kKappaIdx;
static constexpr Dimension kP2SIdx = P1::kNumXDims + P2::kSIdx;
static constexpr Dimension kP3XIdx = P1::kNumXDims + P2::kNumXDims + P3::kPxIdx;
static constexpr Dimension kP3YIdx = P1::kNumXDims + P2::kNumXDims + P3::kPyIdx;
static constexpr Dimension kP3HeadingIdx =
    P1::kNumXDims + P2::kNumXDims + P3::kThetaIdx;
static constexpr Dimension kP3VIdx = P1::kNumXDims + P2::kNumXDims + P3::kVIdx;
static constexpr Dimension kP3SIdx = P1::kNumXDims + P2::kNumXDims + P3::kSIdx;

// Control dimensions.
static constexpr Dimension kP1OmegaIdx = 0;
static constexpr Dimension kP1AIdx = 1;
static constexpr Dimension kP2OmegaIdx = 0;
static constexpr Dimension kP2AIdx = 1;
static constexpr Dimension kP3OmegaIdx = 0;
static constexpr Dimension kP3AIdx = 1;
}  // anonymous namespace

ThreePlayerIntersectionExample::ThreePlayerIntersectionExample()
    : x_idxs_({kP1XIdx, kP2XIdx, kP3XIdx}),
      y_idxs_({kP1YIdx, kP2YIdx, kP3YIdx}),
      heading_idxs_({kP1HeadingIdx, kP2HeadingIdx, kP3HeadingIdx}) {
  // Create dynamics.
  const std::shared_ptr<ConcatenatedDynamicalSystem> dynamics(
      new ConcatenatedDynamicalSystem(
          {std::make_shared<SinglePlayerCar7D>(kInterAxleLength),
           std::make_shared<SinglePlayerCar7D>(kInterAxleLength),
           std::make_shared<SinglePlayerUnicycle5D>()}));

  // Set up initial state.
  x0_ = VectorXf::Zero(dynamics->XDim());
  x0_(kP1XIdx) = kP1InitialX;
  x0_(kP1YIdx) = kP1InitialY;
  x0_(kP1HeadingIdx) = kP1InitialHeading;
  x0_(kP1VIdx) = kP1InitialSpeed;
  x0_(kP2XIdx) = kP2InitialX;
  x0_(kP2YIdx) = kP2InitialY;
  x0_(kP2HeadingIdx) = kP2InitialHeading;
  x0_(kP2VIdx) = kP2InitialSpeed;
  x0_(kP3XIdx) = kP3InitialX;
  x0_(kP3YIdx) = kP3InitialY;
  x0_(kP3HeadingIdx) = kP3InitialHeading;
  x0_(kP3VIdx) = kP3InitialSpeed;

  // Set up initial strategies and operating point.
  strategies_.reset(new std::vector<Strategy>());
  for (PlayerIndex ii = 0; ii < dynamics->NumPlayers(); ii++)
    strategies_->emplace_back(kNumTimeSteps, dynamics->XDim(),
                              dynamics->UDim(ii));

  operating_point_.reset(
      new OperatingPoint(kNumTimeSteps, dynamics->NumPlayers(), dynamics));

  // Set up costs for all players.
  PlayerCost p1_cost, p2_cost, p3_cost;

  // Stay in lanes.
  const Polyline2 lane1(
      {Point2(kP1InitialX, -100.0), Point2(kP1InitialX, 100.0)});
  const Polyline2 lane2({Point2(kP2InitialX, 100.0), Point2(kP2InitialX, 18.0),
                         Point2(kP2InitialX + 0.5, 15.0),
                         Point2(kP2InitialX + 1.0, 14.0),
                         Point2(kP2InitialX + 3.0, 12.5),
                         Point2(kP2InitialX + 6.0, 12.0), Point2(100.0, 12.0)});
  const Polyline2 lane3(
      {Point2(-100.0, kP3InitialY), Point2(100.0, kP3InitialY)});

  const std::shared_ptr<QuadraticPolyline2Cost> p1_lane_cost(
      new QuadraticPolyline2Cost(kLaneCostWeight, lane1, {kP1XIdx, kP1YIdx}));
  const std::shared_ptr<SemiquadraticPolyline2Cost> p1_lane_r_cost(
      new SemiquadraticPolyline2Cost(kLaneBoundaryCostWeight, lane1,
                                     {kP1XIdx, kP1YIdx}, kLaneHalfWidth,
                                     kOrientedRight));
  const std::shared_ptr<SemiquadraticPolyline2Cost> p1_lane_l_cost(
      new SemiquadraticPolyline2Cost(kLaneBoundaryCostWeight, lane1,
                                     {kP1XIdx, kP1YIdx}, -kLaneHalfWidth,
                                     !kOrientedRight));
  p1_cost.AddStateCost(p1_lane_cost);
  p1_cost.AddStateCost(p1_lane_r_cost);
  p1_cost.AddStateCost(p1_lane_l_cost);

  const std::shared_ptr<QuadraticPolyline2Cost> p2_lane_cost(
      new QuadraticPolyline2Cost(kLaneCostWeight, lane2, {kP2XIdx, kP2YIdx}));
  const std::shared_ptr<SemiquadraticPolyline2Cost> p2_lane_r_cost(
      new SemiquadraticPolyline2Cost(kLaneBoundaryCostWeight, lane2,
                                     {kP2XIdx, kP2YIdx}, kLaneHalfWidth,
                                     kOrientedRight));
  const std::shared_ptr<SemiquadraticPolyline2Cost> p2_lane_l_cost(
      new SemiquadraticPolyline2Cost(kLaneBoundaryCostWeight, lane1,
                                     {kP2XIdx, kP2YIdx}, -kLaneHalfWidth,
                                     !kOrientedRight));
  p2_cost.AddStateCost(p2_lane_cost);
  p2_cost.AddStateCost(p2_lane_r_cost);
  p2_cost.AddStateCost(p2_lane_l_cost);

  const std::shared_ptr<QuadraticPolyline2Cost> p3_lane_cost(
      new QuadraticPolyline2Cost(kLaneCostWeight, lane3, {kP3XIdx, kP3YIdx}));
  const std::shared_ptr<SemiquadraticPolyline2Cost> p3_lane_r_cost(
      new SemiquadraticPolyline2Cost(kLaneBoundaryCostWeight, lane3,
                                     {kP3XIdx, kP3YIdx}, kLaneHalfWidth,
                                     kOrientedRight));
  const std::shared_ptr<SemiquadraticPolyline2Cost> p3_lane_l_cost(
      new SemiquadraticPolyline2Cost(kLaneBoundaryCostWeight, lane1,
                                     {kP3XIdx, kP3YIdx}, -kLaneHalfWidth,
                                     !kOrientedRight));
  p3_cost.AddStateCost(p3_lane_cost);
  p3_cost.AddStateCost(p3_lane_r_cost);
  p3_cost.AddStateCost(p3_lane_l_cost);

  // Max/min/nominal speed costs.
  const auto p1_min_v_cost = std::make_shared<SemiquadraticCost>(
      kMaxVCostWeight, kP1VIdx, kMinV, !kOrientedRight);
  const auto p1_max_v_cost = std::make_shared<SemiquadraticCost>(
      kMaxVCostWeight, kP1VIdx, kP1MaxV, kOrientedRight);
  const auto p1_nominal_v_cost = std::make_shared<QuadraticCost>(
      kNominalVCostWeight, kP1VIdx, kP1NominalV);
  p1_cost.AddStateCost(p1_min_v_cost);
  p1_cost.AddStateCost(p1_max_v_cost);
  p1_cost.AddStateCost(p1_nominal_v_cost);

  const auto p2_min_v_cost = std::make_shared<SemiquadraticCost>(
      kMaxVCostWeight, kP2VIdx, kMinV, !kOrientedRight);
  const auto p2_max_v_cost = std::make_shared<SemiquadraticCost>(
      kMaxVCostWeight, kP2VIdx, kP2MaxV, kOrientedRight);
  const auto p2_nominal_v_cost = std::make_shared<QuadraticCost>(
      kNominalVCostWeight, kP2VIdx, kP2NominalV);
  p2_cost.AddStateCost(p2_min_v_cost);
  p2_cost.AddStateCost(p2_max_v_cost);
  p2_cost.AddStateCost(p2_nominal_v_cost);

  const auto p3_min_v_cost = std::make_shared<SemiquadraticCost>(
      kMaxVCostWeight, kP3VIdx, kMinV, !kOrientedRight);
  const auto p3_max_v_cost = std::make_shared<SemiquadraticCost>(
      kMaxVCostWeight, kP3VIdx, kP3MaxV, kOrientedRight);
  const auto p3_nominal_v_cost = std::make_shared<QuadraticCost>(
      kNominalVCostWeight, kP3VIdx, kP3NominalV);
  p3_cost.AddStateCost(p3_min_v_cost);
  p3_cost.AddStateCost(p3_max_v_cost);
  p3_cost.AddStateCost(p3_nominal_v_cost);

  // Curvature costs for P1 and P2.
  const auto p1_curvature_cost =
      std::make_shared<QuadraticCost>(kCurvatureCostWeight, kP1KappaIdx);
  p1_cost.AddStateCost(p1_curvature_cost);

  const auto p2_curvature_cost =
      std::make_shared<QuadraticCost>(kCurvatureCostWeight, kP2KappaIdx);
  p2_cost.AddStateCost(p2_curvature_cost);

  // Penalize control effort.
  const auto p1_omega_cost =
      std::make_shared<QuadraticCost>(kOmegaCostWeight, kP1OmegaIdx);
  const auto p1_a_cost = std::make_shared<QuadraticCost>(kACostWeight, kP1AIdx);
  p1_cost.AddControlCost(0, p1_omega_cost);
  p1_cost.AddControlCost(0, p1_a_cost);

  const auto p2_omega_cost =
      std::make_shared<QuadraticCost>(kOmegaCostWeight, kP2OmegaIdx);
  const auto p2_a_cost = std::make_shared<QuadraticCost>(kACostWeight, kP2AIdx);
  p2_cost.AddControlCost(1, p2_omega_cost);
  p2_cost.AddControlCost(1, p2_a_cost);

  const auto p3_omega_cost =
      std::make_shared<QuadraticCost>(kOmegaCostWeight, kP3OmegaIdx);
  const auto p3_a_cost = std::make_shared<QuadraticCost>(kACostWeight, kP3AIdx);
  p3_cost.AddControlCost(2, p3_omega_cost);
  p3_cost.AddControlCost(2, p3_a_cost);

  // Path lenth costs.
  const auto p1_s_cost = std::make_shared<NominalPathLengthCost>(
      kSCostWeight, kP1SIdx, kP1NominalV);
  p1_cost.AddStateCost(p1_s_cost);

  const auto p2_s_cost = std::make_shared<NominalPathLengthCost>(
      kSCostWeight, kP2SIdx, kP2NominalV);
  p2_cost.AddStateCost(p2_s_cost);

  const auto p3_s_cost = std::make_shared<NominalPathLengthCost>(
      kSCostWeight, kP3SIdx, kP3NominalV);
  p3_cost.AddStateCost(p3_s_cost);

  // Goal costs.
  const auto p1_goalx_cost =
      std::make_shared<QuadraticCost>(kGoalCostWeight, kP1XIdx, kP1GoalX);
  const auto p1_goaly_cost =
      std::make_shared<QuadraticCost>(kGoalCostWeight, kP1YIdx, kP1GoalY);
  p1_cost.AddStateCost(p1_goalx_cost);
  p1_cost.AddStateCost(p1_goaly_cost);

  const auto p2_goalx_cost =
      std::make_shared<QuadraticCost>(kGoalCostWeight, kP2XIdx, kP2GoalX);
  const auto p2_goaly_cost =
      std::make_shared<QuadraticCost>(kGoalCostWeight, kP2YIdx, kP2GoalY);
  p2_cost.AddStateCost(p2_goalx_cost);
  p2_cost.AddStateCost(p2_goaly_cost);

  const auto p3_goalx_cost =
      std::make_shared<QuadraticCost>(kGoalCostWeight, kP3XIdx, kP3GoalX);
  const auto p3_goaly_cost =
      std::make_shared<QuadraticCost>(kGoalCostWeight, kP3YIdx, kP3GoalY);
  p3_cost.AddStateCost(p3_goalx_cost);
  p3_cost.AddStateCost(p3_goaly_cost);

  // Set up solver.
  solver_.reset(new ILQSolver(dynamics, {p1_cost, p2_cost, p3_cost},
                              kTimeHorizon, kTimeStep));
}

}  // namespace ilqgames
