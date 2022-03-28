#ifndef H3_MODEL_H
#define H3_MODEL_H

#include <towr/models/kinematic_model.h>
#include <towr/models/single_rigid_body_dynamics.h>
#include <towr/models/endeffector_mappings.h>

namespace towr {

/**
 * @brief The Kinematics of H3 (TODO: Real Values)
 */
class H3KinematicModel : public KinematicModel {
public:
  H3KinematicModel () : KinematicModel(2)
  {
    const double z_nominal_b = -0.9;
    const double y_nominal_b =  0.10;

    nominal_stance_.at(L) << 0.0,  y_nominal_b, z_nominal_b;
    nominal_stance_.at(R) << 0.0, -y_nominal_b, z_nominal_b;

    max_dev_from_nominal_  << 0.18, 0.10, 0.060;
  }
};

/**
 * @brief The Dynamics of a H3
 */
class H3DynamicModel : public SingleRigidBodyDynamics {
public:
  H3DynamicModel()
  : SingleRigidBodyDynamics(93.3357,
			    15.4261, 12.6152, 4.54076,
			    -0.00456485, 1.27926, 0.0611941, 
			    2) {}
};

} /* namespace towr */

#endif /* end of include guard: H3_MODEL_H */
