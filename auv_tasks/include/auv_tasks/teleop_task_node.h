/***************************************************************************
 *  include/auv_tasks/teleop_task_node.h
 *  --------------------
 *
 *  Copyright (c) 2013, Dylan Foster
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of USC AUV nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **************************************************************************/

/// ROS
#include <ros/ros.h>

/// generic task
#include <auv_tasks/task_executor.h>
#include <auv_tasks/teleop_policy.h>

/// 
/* #include <uscauv_utilities/pose_integrator.h> */

/// transforms
#include <tf/transform_listener.h>

/// Seabee
#include <seabee3_msgs/MotorVals.h>
#include <seabee3_common/movement.h>

typedef seabee3_msgs::MotorVals _MotorValsMsg;

using namespace seabee3_common;

/**
 * To Implement:
 * Function/Option to normalize the motor vals for an axis
 * Scale for axes (within teleop policy)
 * Axis motor setter function within seabee3 movement
 * Some sort of generic axis assigner that will work with the new robot without changing anything in this project
 * Sensor policy? Get IMU, pressure
 * Pose Integrator
 */

class TeleopTaskNode: public TaskExecutorNode, public TeleopPolicy
{
 private:
  /// ROS interfaces
  ros::NodeHandle nh_rel_;
  tf::TransformListener tf_listener_;
  ros::Publisher motor_vals_pub_;
  
 private:
  std::string imu_frame_name_;

  /* PoseIntegrator pose_integrator_; */
  
 public:
 TeleopTaskNode(): TeleopPolicy("teleop_task"){}

 private:
  void spinFirst()
  {
    /// all setpoints are initializes to zero
    controller_.init("linear/x", "linear/y", "linear/z",
    		     "angular/yaw", "angular/pitch", "angular/roll");

    init();

    /// TODO: Load this as param
    imu_frame_name_ = "/seabee3/sensors/imu";

    nh_rel_ = ros::NodeHandle("~");
    
    motor_vals_pub_ = nh_rel_.advertise<_MotorValsMsg>("motor_vals", 1);
    
  }

  void spinOnce()
  {
    /* pose_integrator_.updatePose(); */

    if( tf_listener_.canTransform( "/world", imu_frame_name_, ros::Time(0) ))
      {
	tf::StampedTransform world_to_imu_tf;
	    
	try
	  {
	    tf_listener_.lookupTransform( "/world", imu_frame_name_, ros::Time(0), world_to_imu_tf);

	  }
	catch(tf::TransformException & ex)
	  {
	    ROS_ERROR( "%s", ex.what() );
	    return;
	  }

	double roll, pitch, yaw;
	    
	/* Yaw around Z, pitch around Y, roll around X */
	world_to_imu_tf.getBasis().getEulerZYX( yaw, pitch, roll );
	    
	roll *= 180 / M_PI;
	pitch *= 180 / M_PI;
	yaw *= 180 / M_PI;
	   

	/// TODO: Add enum-type thing to PID6D such that axes 0-6 are called PID6D::YAW etc.
	/* controller_.setObserved<3>(yaw); */
	controller_.setObserved<4>(pitch);
	controller_.setObserved<5>(roll);
	    
      }

    if( getButtonByName("enable") )
      {
	/// Apply setpoints from joystick

	/// Rack-and-pinion-style yaw control
	controller_.setSetpoint<0>( getAxisByName("linear.x")*100 );
	controller_.setSetpoint<1>( getAxisByName("linear.y")*100 );
	controller_.setSetpoint<2>( getAxisByName("linear.z")*100 );
	
	controller_.setSetpoint<3>( getAxisByName("angular.z")*180 );
	
	if( getButtonByIndex(0))
	  {
	    ROS_INFO("Doing a barrel roll...");
 	    controller_.setSetpoint<5>(getButtonByIndex(0)*180);
 	    controller_.setObserved<5>(0);
	  }
	geometry_msgs::Twist twist;
	
	twist.linear.z = getAxisByName("linear.z");

	/* pose_integrator_.setVelocity(twist); */
		
      }
    else
      {
	controller_.setSetpoint<0>(0);
	controller_.setSetpoint<1>(0);
	controller_.setSetpoint<2>(0);

	controller_.setSetpoint<3>(0);

	controller_.setSetpoint<5>(0);
	
      }
	
    publishMotors();
  }

 private:
  void publishMotors()
  {
    _MotorValsMsg msg;
    
    applyMotors(msg, movement::Axes::SPEED, controller_.update<0>() );
    applyMotors(msg, movement::Axes::STRAFE, controller_.update<1>() );
    applyMotors(msg, movement::Axes::DEPTH, controller_.update<2>() );
    applyMotors(msg, movement::Axes::YAW, controller_.update<3>() );
    applyMotors(msg, movement::Axes::PITCH, controller_.update<4>() );
    applyMotors(msg, movement::Axes::ROLL, controller_.update<5>() );
    
    /// TODO: THrottle down to 100 here so that the sum is < 100
    motor_vals_pub_.publish( msg );
    
  }

  /// TODO:Redo this entire function
  void applyMotors(_MotorValsMsg & msg, int const & axis, double val)
  {
    if( val > 100)
      val = 100;
    else if( val < -100)
      val = -100;

    int motor1_id = movement::ThrusterPairs::values[axis][0];
    int motor2_id = movement::ThrusterPairs::values[axis][1];

    msg.mask[motor1_id] = 1;
    msg.mask[motor2_id] = 1;
    
    switch(axis)
      {
      case movement::Axes::YAW:
	{
	  msg.motors[motor1_id] += val;
	  msg.motors[motor2_id] += -val;
	  break;
	}
      case movement::Axes::PITCH:
	{
	  msg.motors[motor1_id] += -val;
	  msg.motors[motor2_id] += val;
	  break;
	}
      case movement::Axes::ROLL:
	{
	  msg.motors[motor1_id] += -val;
	  msg.motors[motor2_id] += -val;
	  break;
	}
      case movement::Axes::SPEED:
	{
	  msg.motors[motor1_id] += val;
	  msg.motors[motor2_id] += val;
	  break;
	}
      case movement::Axes::STRAFE:
	{
	  msg.motors[motor1_id] += val;
	  msg.motors[motor2_id] += -val;
	  break;
	}
      case movement::Axes::DEPTH:
	{
	  msg.motors[motor1_id] += -val;
	  msg.motors[motor2_id] += -val;
	  break;
	}
      }
    
  }



};
