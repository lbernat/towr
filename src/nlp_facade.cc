/*
 * nlp_optimizer.cpp
 *
 *  Created on: Mar 18, 2016
 *      Author: winklera
 */

#include <xpp/zmp/nlp_facade.h>

#include <xpp/zmp/spline_container.h>
#include <xpp/zmp/continuous_spline_container.h>
#include <xpp/hyq/support_polygon_container.h>
#include <xpp/zmp/initial_acceleration_equation.h>
#include <xpp/zmp/final_state_equation.h>
#include <xpp/zmp/spline_junction_equation.h>
#include <xpp/zmp/ipopt_adapter.h>
// this looks like i need the factory method
#include <xpp/zmp/a_linear_constraint.h>
#include <xpp/zmp/zmp_constraint.h>
#include <xpp/zmp/range_of_motion_constraint.h>
#include <xpp/zmp/joint_angles_constraint.h>
#include <xpp/hyq/hyq_inverse_kinematics.h>
// cost function stuff
#include <xpp/zmp/a_quadratic_cost.h>
#include <xpp/zmp/range_of_motion_cost.h>
#include <xpp/zmp/total_acceleration_equation.h>

#include <xpp/zmp/interpreting_observer.h>

namespace xpp {
namespace zmp {

NlpFacade::NlpFacade (IVisualizer& visualizer)
     :visualizer_(&visualizer)
{
  interpreting_observer_ = std::make_shared<InterpretingObserver>(opt_variables_);

  constraints_.AddConstraint(std::make_shared<LinearEqualityConstraint>(opt_variables_), "acc");
  constraints_.AddConstraint(std::make_shared<LinearEqualityConstraint>(opt_variables_), "final");
  constraints_.AddConstraint(std::make_shared<LinearEqualityConstraint>(opt_variables_), "junction");
  constraints_.AddConstraint(std::make_shared<ZmpConstraint>(opt_variables_), "zmp");
  constraints_.AddConstraint(std::make_shared<RangeOfMotionConstraint>(opt_variables_), "rom");
//  constraints_.AddConstraint(std::make_shared<JointAnglesConstraint>(opt_variables_), "joint_angles");

  costs_.AddCost(std::make_shared<AQuadraticCost>(opt_variables_), "cost_acc");
  costs_.AddCost(std::make_shared<RangeOfMotionCost>(opt_variables_), "cost_rom");

  // initialize the ipopt solver
  ipopt_solver_.RethrowNonIpoptException(true); // this allows to see the error message of exceptions thrown inside ipopt
  status_ = ipopt_solver_.Initialize();
  if (status_ != Ipopt::Solve_Succeeded) {
    std::cout << std::endl << std::endl << "*** Error during initialization!" << std::endl;
    throw std::length_error("Ipopt could not initialize correctly");
  }
}

void
NlpFacade::InitializeVariables (const Eigen::VectorXd& spline_abcd_coeff,
                                const StdVecEigen2d& footholds)
{
  opt_variables_.Init(spline_abcd_coeff, footholds);
  opt_variables_initialized_ = true;
}

void
NlpFacade::InitializeVariables (int n_spline_coeff, int n_footholds)
{
  opt_variables_.Init(n_spline_coeff, n_footholds);
  opt_variables_initialized_ = true;
}

void
NlpFacade::SolveNlp(const Eigen::Vector2d& initial_acc,
                    const State& final_state,
                    const InterpreterPtr& interpreter_ptr)
{
  assert(opt_variables_initialized_);
  // save the framework of the optimization problem
  interpreting_observer_->SetInterpreter(interpreter_ptr);

  xpp::hyq::SupportPolygonContainer supp_polygon_container;
  supp_polygon_container.Init(interpreter_ptr->GetStartStance(),
                              interpreter_ptr->GetStepSequence(),
                              xpp::hyq::SupportPolygon::GetDefaultMargins());

  ContinuousSplineContainer spline_structure = interpreter_ptr->GetSplineStructure();

  // This should all be hidden inside a factory method
  // the linear equations
  InitialAccelerationEquation eq_acc(initial_acc, spline_structure.GetTotalFreeCoeff());
  FinalStateEquation eq_final(final_state, spline_structure);
  SplineJunctionEquation eq_junction(spline_structure);

  // initialize the constraints
  // fixme bad practice, remove
  xpp::hyq::HyqInverseKinematics hyq_inv_kin;

  dynamic_cast<LinearEqualityConstraint&>(constraints_.GetConstraint("acc")).Init(eq_acc.BuildLinearEquation());
  dynamic_cast<LinearEqualityConstraint&>(constraints_.GetConstraint("final")).Init(eq_final.BuildLinearEquation());
  dynamic_cast<LinearEqualityConstraint&>(constraints_.GetConstraint("junction")).Init(eq_junction.BuildLinearEquation());
  dynamic_cast<ZmpConstraint&>(constraints_.GetConstraint("zmp")).Init(spline_structure, supp_polygon_container, interpreter_ptr->GetRobotHeight());
  dynamic_cast<RangeOfMotionConstraint&>(constraints_.GetConstraint("rom")).Init(spline_structure, supp_polygon_container);
//  dynamic_cast<JointAnglesConstraint&>(constraints_.GetConstraint("joint_angles")).Init(*interpreter_ptr, &hyq_inv_kin);
  constraints_.Refresh();

  // costs
  TotalAccelerationEquation eq_total_acc(spline_structure);
  dynamic_cast<AQuadraticCost&>(costs_.GetCost("cost_acc")).Init(eq_total_acc.BuildLinearEquation());
  dynamic_cast<RangeOfMotionCost&>(costs_.GetCost("cost_rom")).Init(spline_structure, supp_polygon_container);

  // todo create complete class out of these input arguments
  IpoptPtr nlp_ptr = new Ipopt::IpoptAdapter(opt_variables_,
                                             costs_,
                                             constraints_,
                                             *visualizer_); // just so it can poll the PublishMsg() method
  SolveIpopt(nlp_ptr);
  opt_variables_initialized_ = false;
}

void
NlpFacade::SolveIpopt (const IpoptPtr& nlp)
{
  status_ = ipopt_solver_.OptimizeTNLP(nlp);
  if (status_ == Ipopt::Solve_Succeeded) {
    // Retrieve some statistics about the solve
    Ipopt::Index iter_count = ipopt_solver_.Statistics()->IterationCount();
    std::cout << std::endl << std::endl << "*** The problem solved in " << iter_count << " iterations!" << std::endl;

    Ipopt::Number final_obj = ipopt_solver_.Statistics()->FinalObjective();
    std::cout << std::endl << std::endl << "*** The final value of the objective function is " << final_obj << '.' << std::endl;
  }
}

NlpFacade::InterpretingObserverPtr
NlpFacade::GetObserver () const
{
  return interpreting_observer_;
}

void
NlpFacade::AttachVisualizer (IVisualizer& visualizer)
{
  visualizer_ = &visualizer;
}

NlpFacade::VecFoothold
NlpFacade::GetFootholds () const
{
  return interpreting_observer_->GetFootholds();
}

NlpFacade::VecSpline
NlpFacade::GetSplines () const
{
  return interpreting_observer_->GetSplines();
}

} /* namespace zmp */
} /* namespace xpp */

