/*******************************************************************************
 *
 *      xsens_node
 * 
 *      Copyright (c) 2010, Edward T. Kaszubski (ekaszubski@gmail.com)
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
#include <queue> // for queue
#include <ros/ros.h>
#include <tf/tf.h> // for tf::Vector3
#include <xsens/XSensDriver.h> // for XSensDriver
#include <geometry_msgs/Vector3.h>
#include <math.h> //for pow, sqrt
//msgs
#include <sensor_msgs/Imu.h> // for outgoing IMU data
#include <std_msgs/Bool.h> //for Bool
//srvs
#include <xsens_node/CalibrateRPY.h> // for CalibrateRPY
#include <std_srvs/Empty.h> //for Empty
void operator +=( XSensDriver::Vector3 & v1, tf::Vector3 & v2 )
{
	v1.x += v2.getX();
	v1.y += v2.getY();
	v1.z += v2.getZ();
}

void operator >>( XSensDriver::Vector3 & v1, geometry_msgs::Vector3 & v2 )
{
	v2.x = v1.x;
	v2.y = v1.y;
	v2.z = v1.z;
}

void operator >>( XSensDriver::Vector3 & v1, tf::Vector3 & v2 )
{
	v2.setX( v1.x );
	v2.setY( v1.y );
	v2.setZ( v1.z );
}

void operator >>( tf::Vector3 & v1, geometry_msgs::Vector3 & v2 )
{
	v2.x = v1.getX();
	v2.y = v1.getY();
	v2.z = v1.getZ();
}

class XSensNode
{
private:
	std::queue<tf::Vector3> ori_data_cache_;
	tf::Vector3 ori_comp_, drift_comp_, drift_comp_total_;

	std::string port_, frame_id_;
	bool calibrated_, autocalibrate_, assume_calibrated_;
	double orientation_stdev_, angular_velocity_stdev_, linear_acceleration_stdev_, max_drift_rate_;

	ros::NodeHandle n_priv_;
	ros::Publisher imu_pub_;
	ros::Publisher is_calibrated_pub_;
	ros::ServiceServer calibrate_rpy_drift_srv_;
	ros::ServiceServer calibrate_rpy_ori_srv_;

	XSensDriver * imu_driver_;
	const static unsigned int IMU_DATA_CACHE_SIZE = 2;

public:
	XSensNode( ros::NodeHandle & n ) :
		n_priv_( "~" )
	{
		n_priv_.param( "port", port_, std::string( "/dev/ttyUSB0" ) );
		n_priv_.param( "frame_id", frame_id_, std::string( "imu" ) );
		n_priv_.param( "autocalibrate", autocalibrate_, true );
		n_priv_.param( "orientation_stdev", orientation_stdev_, 0.035 );
		n_priv_.param( "angular_velocity_stdev", angular_velocity_stdev_, 0.012 );
		n_priv_.param( "linear_acceleration_stdev", linear_acceleration_stdev_, 0.098 );
		n_priv_.param( "max_drift_rate", max_drift_rate_, 0.0001 );
		n_priv_.param( "assume_calibrated", assume_calibrated_, false );

		calibrated_ = false;

		imu_pub_ = n.advertise<sensor_msgs::Imu> ( "data", 1 );
		is_calibrated_pub_ = n.advertise<std_msgs::Bool> ( "is_calibrated", 1 );
		calibrate_rpy_drift_srv_ = n.advertiseService( "calibrate_rpy_drift", &XSensNode::calibrateRPYDriftCB, this );
		calibrate_rpy_ori_srv_ = n.advertiseService( "calibrate_rpy_ori", &XSensNode::calibrateRPYOriCB, this );

		imu_driver_ = new XSensDriver();
		if ( !imu_driver_->initMe() )
		{
			ROS_WARN( "Failed to connect to IMU. Exiting..." );
			return;
		}
	}

	~XSensNode()
	{
		delete imu_driver_;
	}

	void updateIMUData()
	{
		if ( !imu_driver_->updateData() )
		{
			ROS_WARN( "Failed to update data during this cycle..." );
		}
		else
		{
			tf::Vector3 temp;
			imu_driver_->ori_ >> temp;

			while ( ori_data_cache_.size() >= IMU_DATA_CACHE_SIZE )
			{
				ori_data_cache_.pop();
			}

			ori_data_cache_.push( temp );
		}
	}

	void runFullCalibration()
	{
		runRPYOriCalibration();
		runRPYDriftCalibration();
		checkCalibration();
	}

	void checkCalibration()
	{
		double drift_rate = sqrt( pow( drift_comp_.x(), 2 ) + pow( drift_comp_.y(), 2 ) + pow( drift_comp_.z(), 2 ) );
		calibrated_ = drift_rate > max_drift_rate_;

		std_msgs::Bool is_calibrated_msg;
		is_calibrated_msg.data = calibrated_;
		is_calibrated_pub_.publish( is_calibrated_msg );
	}

	void runRPYOriCalibration( int n = 50 )
	{
		ori_comp_ *= 0.0; //reset the vector to <0, 0, 0>
		for ( int i = 0; i < n && ros::ok(); i++ )
		{
			updateIMUData();
			ori_comp_ += ori_data_cache_.front();
			ros::spinOnce();
			ros::Rate( 110 ).sleep();
		}

		ori_comp_ /= (double) ( -n );
	}

	void runRPYDriftCalibration( int n = 50 )
	{
		drift_comp_ *= 0.0; //reset the vector to <0, 0, 0>
		updateIMUData();
		drift_comp_ = ori_data_cache_.front();
		for ( int i = 0; i < n && ros::ok(); i++ )
		{
			updateIMUData();
			ros::spinOnce();
			ros::Rate( 110 ).sleep();
		}
		drift_comp_ -= ori_data_cache_.front();
		drift_comp_ /= (double) ( n ); //avg drift per cycle
	}

	bool calibrateRPYOriCB( xsens_node::CalibrateRPY::Request &req, xsens_node::CalibrateRPY::Response &res )
	{
		runRPYOriCalibration( req.num_samples );

		ori_comp_ >> res.calibration;

		checkCalibration();

		return true;
	}

	bool calibrateRPYDriftCB( xsens_node::CalibrateRPY::Request &req, xsens_node::CalibrateRPY::Response &res )
	{
		runRPYDriftCalibration( req.num_samples );

		drift_comp_ >> res.calibration;

		checkCalibration();

		return true;
	}

	bool calibrateCB( std_srvs::Empty::Request & req, std_srvs::Empty::Response & res )
	{
		runFullCalibration();
	}

	void spin()
	{
		while ( ros::ok() )
		{
//			if ( autocalibrate_ && !calibrated_ )
//				runFullCalibration();

			sensor_msgs::Imu msg;

			updateIMUData();

			imu_driver_->accel_ >> msg.linear_acceleration;
			imu_driver_->gyro_ >> msg.angular_velocity;
			//imu_driver_->mag_ >> msg.mag;

			//imu_driver_->accel_ >> msg_calib.accel;
			//imu_driver_->gyro_ >> msg_calib.gyro;
			//imu_driver_->mag_ >> msg_calib.mag;

			drift_comp_total_ += drift_comp_;

			imu_driver_->ori_ += drift_comp_total_;

			tf::Vector3 temp;
			imu_driver_->ori_ >> temp;
			temp += ori_comp_;

			tf::Quaternion ori ( temp.z(), temp.y(), temp.x() );

			msg.orientation.w = ori.w();
			msg.orientation.x = ori.x();
			msg.orientation.y = ori.y();
			msg.orientation.z = ori.z();

			msg.angular_velocity_covariance[0] = msg.angular_velocity_covariance[4] = msg.angular_velocity_covariance[8] = angular_velocity_stdev_ * angular_velocity_stdev_;
			msg.linear_acceleration_covariance[0] = msg.linear_acceleration_covariance[4] = msg.linear_acceleration_covariance[8] = linear_acceleration_stdev_ * linear_acceleration_stdev_;
			msg.orientation_covariance[0] = msg.orientation_covariance[4] = msg.orientation_covariance[8] = orientation_stdev_ * orientation_stdev_;

			imu_pub_.publish( msg );
			//imu_pub_raw_.publish( msg_raw );
			ros::spinOnce();
			ros::Rate( 110 ).sleep();
		}
	}
};

int main( int argc, char** argv )
{
	ros::init( argc, argv, "xsens_node" );
	ros::NodeHandle n;

	XSensNode xsens_node( n );
	xsens_node.spin();
	
	return 0;
}