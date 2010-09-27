/*******************************************************************************
 *
 *      seabee3_driver_base
 * 
 *      Copyright (c) 2010,
 *      Edward T. Kaszubski (ekaszubski@gmail.com)
 *      All rights reserved.
 *
 *      Redistribution and use in source and binary forms, with or without
 *      modification, are permitted provided that the following conditions are
 *      met:
 *      
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following disclaimer
 *        in the documentation and/or other materials provided with the
 *        distribution.
 *      * Neither the name of the USC Underwater Robotics Team nor the names of its
 *        contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *      
 *      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *      "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *      LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *      A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *      OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *      SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *      LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *      DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *      THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *      (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *      OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************************/

//tools
#include <ros/ros.h>
#include <seabee3_beestem/BeeStem3Driver.h> //for BeeStem3Driver
#include <string>
//msgs
#include <seabee3_driver_base/Depth.h> // for outgoing Depth
#include <seabee3_driver_base/KillSwitch.h> // for outgoing KillSwitch
#include <seabee3_driver_base/MotorCntl.h> // for incoming MotorCntl
#include <seabee3_driver_base/Pressure.h> // for outgoing Pressure
//srvs
#include <seabee3_driver_base/FiringDeviceAction.h> // for FiringDeviceAction

class Seabee3DriverBaseNode
{
private:
	ros::NodeHandle n_priv_;
	ros::Subscriber motor_cntl_sub_;

	ros::Publisher intl_pressure_pub_;
	ros::Publisher extl_pressure_pub_;
	ros::Publisher depth_pub_;
	ros::Publisher kill_switch_pub_;

	ros::ServiceServer dropper1_action_srv_;
	ros::ServiceServer dropper2_action_srv_;
	ros::ServiceServer shooter_action_srv_;

	BeeStem3Driver * bee_stem_3_driver_;
	std::string port_;
	int surface_pressure_;
	bool pressure_calibrated_;
public:
	//#define SURFACE_PRESSURE 908
	const static int PRESSURE_DEPTH_SLOPE = 33;

	Seabee3DriverBaseNode( ros::NodeHandle & n ) :
		n_priv_( "~" )
	{
		pressure_calibrated_ = false;

		n_priv_.param( "surface_pressure", surface_pressure_, 908 );
		n_priv_.param( "port", port_, std::string("/dev/ttyUSB0") );

		ROS_INFO( "constructing new driver instance" );
		bee_stem_3_driver_ = new BeeStem3Driver( port_ );

		n_priv_.param( "shooter/trigger_time", bee_stem_3_driver_->shooter_params_.trigger_time_, 50 );
		n_priv_.param( "shooter/trigger_value", bee_stem_3_driver_->shooter_params_.trigger_value_, 80 );

		n_priv_.param( "dropper1/trigger_time", bee_stem_3_driver_->dropper1_params_.trigger_time_, 50 );
		n_priv_.param( "dropper1/trigger_value", bee_stem_3_driver_->dropper1_params_.trigger_value_, 40 );

		n_priv_.param( "dropper2/trigger_time", bee_stem_3_driver_->dropper2_params_.trigger_time_, 50 );
		n_priv_.param( "dropper2/trigger_value", bee_stem_3_driver_->dropper2_params_.trigger_value_, 40 );

		motor_cntl_sub_ = n.subscribe( "/seabee3/motor_cntl", 1, &Seabee3DriverBaseNode::motorCntlCB, this );

		intl_pressure_pub_ = n.advertise<seabee3_driver_base::Pressure> ( "/seabee3/intl_pressure", 1 );
		extl_pressure_pub_ = n.advertise<seabee3_driver_base::Pressure> ( "/seabee3/extl_pressure", 1 );
		depth_pub_ = n.advertise<seabee3_driver_base::Depth> ( "/seabee3/depth", 1 );
		kill_switch_pub_ = n.advertise<seabee3_driver_base::KillSwitch> ( "/seabee3/kill_switch", 1 );

		dropper1_action_srv_ = n.advertiseService( "/seabee3/dropper1_action", &Seabee3DriverBaseNode::dropper1ActionCB, this );
		dropper2_action_srv_ = n.advertiseService( "/seabee3/dropper2_action", &Seabee3DriverBaseNode::dropper2ActionCB, this );
		shooter_action_srv_ = n.advertiseService( "/seabee3/shooter_action", &Seabee3DriverBaseNode::shooterActionCB, this );
	}

	float getDepthFromPressure( int pressure )
	{
		return (float) ( pressure - surface_pressure_ ) / PRESSURE_DEPTH_SLOPE;
	}

	void motorCntlCB( const seabee3_driver_base::MotorCntlConstPtr & msg )
	{
		for ( unsigned int i = 0; i < msg->motors.size(); i++ )
		{
			if ( msg->mask[i] == 1 ) bee_stem_3_driver_->bee_stem_3_->setThruster( i, msg->motors[i] );
		}
	}

	bool executeFiringDeviceAction( seabee3_driver_base::FiringDeviceAction::Request &req, seabee3_driver_base::FiringDeviceAction::Response &res, int device_id )
	{
		bool & device_status = bee_stem_3_driver_->getDeviceStatus( device_id );
		switch ( req.action )
		{
		case seabee3_driver_base::FiringDeviceAction::Request::CHECK_STATUS:
			break;
		case seabee3_driver_base::FiringDeviceAction::Request::RESET_STATUS:
			device_status = true;
			break;
		case seabee3_driver_base::FiringDeviceAction::Request::FIRE:
			bee_stem_3_driver_->fireDevice( device_id );
			break;
		}
		res.is_loaded = device_status;
		return true;
	}

	bool dropper1ActionCB( seabee3_driver_base::FiringDeviceAction::Request &req, seabee3_driver_base::FiringDeviceAction::Response &res )
	{
		return executeFiringDeviceAction( req, res, BeeStem3Driver::FiringDeviceID::DropperStage1 );
	}

	bool dropper2ActionCB( seabee3_driver_base::FiringDeviceAction::Request &req, seabee3_driver_base::FiringDeviceAction::Response &res )
	{
		return executeFiringDeviceAction( req, res, BeeStem3Driver::FiringDeviceID::DropperStage2 );
	}

	bool shooterActionCB( seabee3_driver_base::FiringDeviceAction::Request &req, seabee3_driver_base::FiringDeviceAction::Response &res )
	{
		return executeFiringDeviceAction( req, res, BeeStem3Driver::FiringDeviceID::Shooter );
	}

	void spin()
	{
		while ( ros::ok() )
		{
			seabee3_driver_base::Pressure intl_pressure_msg;
			seabee3_driver_base::Pressure extl_pressure_msg;
			seabee3_driver_base::Depth depth_msg;
			seabee3_driver_base::KillSwitch kill_switch_msg;

			bee_stem_3_driver_->readPressure( intl_pressure_msg.value, extl_pressure_msg.value );

			if ( !pressure_calibrated_ )
			{
				surface_pressure_ = extl_pressure_msg.value;
				pressure_calibrated_ = true;
			}

			bee_stem_3_driver_->readKillSwitch( kill_switch_msg.is_killed );
			depth_msg.value = getDepthFromPressure( extl_pressure_msg.value );

			intl_pressure_pub_.publish( intl_pressure_msg );
			extl_pressure_pub_.publish( extl_pressure_msg );
			depth_pub_.publish( depth_msg );
			kill_switch_pub_.publish( kill_switch_msg );

			ros::spinOnce();
			ros::Rate( 20 ).sleep();
		}
	}

};

int main( int argc, char** argv )
{
	ros::init( argc, argv, "seabee3_driver_base" );
	ros::NodeHandle n;
	
	Seabee3DriverBaseNode seabee3_driver_base_node( n );
	seabee3_driver_base_node.spin();

	return 0;
}