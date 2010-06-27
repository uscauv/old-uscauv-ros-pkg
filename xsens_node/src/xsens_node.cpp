#include <xsens/xsens_driver.h>

#include <xsens_node/IMUData.h>
#include <xsens_node/CalibrateRPYOri.h>
#include <xsens_node/CalibrateRPYDrift.h>

#include <ros/ros.h>
#include <tf/tf.h>
#include <iostream>
#include <queue>

std::queue<tf::Vector3> * ori_data_cache;
tf::Vector3 * ori_comp;
tf::Vector3 * drift_comp;
tf::Vector3 * drift_comp_total;
//tf::Vector3 * rpy_zero_total;
XSensDriver * mImuDriver;
double * sampleTime;
const static int IMU_DATA_CACHE_SIZE = 2;

void operator += (XSensDriver::Vector3 & v1, tf::Vector3 & v2)
{
	v1.x += v2.getX();
	v1.y += v2.getY();
	v1.z += v2.getZ();
}

void operator >> (XSensDriver::Vector3 & v1, geometry_msgs::Vector3 & v2)
{
	v2.x = v1.x;
	v2.y = v1.y;
	v2.z = v1.z;
}

void operator >> (XSensDriver::Vector3 & v1, tf::Vector3 & v2)
{
	v2.setX(v1.x);
	v2.setY(v1.y);
	v2.setZ(v1.z);
}

void operator >> (tf::Vector3 & v1, geometry_msgs::Vector3 & v2)
{
	v2.x = v1.getX();
	v2.y = v1.getY();
	v2.z = v1.getZ();
}

void updateIMUData()
{
	if( !mImuDriver->updateData() )
	{
		ROS_WARN("Failed to update data during this cycle...");
		//fprintf(stdout, "-");
	}
	else
	{
		//fprintf(stdout, ".");
		//std::cout << std::flush;
		tf::Vector3 temp;
		mImuDriver->ori >> temp;
		
		while(ori_data_cache->size() >= IMU_DATA_CACHE_SIZE)
		{
			ori_data_cache->pop();
		}
		
		ori_data_cache->push(temp);
	}
}

void runRPYOriCalibration(int n = 10)
{
	*ori_comp *= 0.0; //reset the vector to <0, 0, 0>
	for(int i = 0; i < n && ros::ok(); i ++)
	{
		updateIMUData();
		*ori_comp += ori_data_cache->front();
		ros::spinOnce();
		ros::Rate(110).sleep();
		//ROS_INFO("sample %d: x %f y %f z %f", i, diff.getX(), diff.getY(), diff.getZ());
	}
	
	*ori_comp /= (double)(-n);
}

void runRPYDriftCalibration(int n = 10)
{
	*drift_comp *= 0.0; //reset the vector to <0, 0, 0>
	updateIMUData();
	*drift_comp = ori_data_cache->front();
	for(int i = 0; i < n && ros::ok(); i ++)
	{
		updateIMUData();
		ros::spinOnce();
		ros::Rate(110).sleep();
		//ROS_INFO("sample %d: x %f y %f z %f", i, diff.getX(), diff.getY(), diff.getZ());
	}
	*drift_comp -= ori_data_cache->front();
	
	//*sampleTime = (double)n / 110.0;
	
	*drift_comp /= (double)(n); //avg drift per cycle
}

bool CalibrateRPYOriCallback (xsens_node::CalibrateRPYOri::Request &req, xsens_node::CalibrateRPYOri::Response &res)
{
	int numSamples = req.ZeroReq;
	
	runRPYOriCalibration(numSamples);
	
	*ori_comp >> res.Result;
	
	return true;
}

bool CalibrateRPYDriftCallback (xsens_node::CalibrateRPYDrift::Request &req, xsens_node::CalibrateRPYDrift::Response &res)
{
	int numSamples = req.ZeroReq;
	
	runRPYDriftCalibration(numSamples);
	
	*drift_comp >> res.Result;
	
	return true;
}

int main(int argc, char** argv)
{
	ros::init(argc, argv, "xsens_node");
	ros::NodeHandle n;
	ros::Publisher imu_pub = n.advertise<xsens_node::IMUData>("/xsens/IMUData", 1);
	ros::ServiceServer CalibrateRPYDrift_srv = n.advertiseService("/xsens/CalibrateRPYDrift", CalibrateRPYDriftCallback);
	ros::ServiceServer CalibrateRPYOri_srv = n.advertiseService("/xsens/CalibrateRPYOri", CalibrateRPYOriCallback);
	
	sampleTime = new double(1.0);
	ori_comp = new tf::Vector3(0, 0, 0);
	drift_comp = new tf::Vector3(0, 0, 0);
	drift_comp_total = new tf::Vector3(0, 0, 0);
	
	ori_data_cache = new std::queue<tf::Vector3>;
	
	int usbIndex;
	n.param("/xsens/usbIndex", usbIndex, 2);
	
	mImuDriver = new XSensDriver((unsigned int)usbIndex);
	if( !mImuDriver->initMe() )
	{
		ROS_WARN("Failed to connect to IMU. Exiting...");
		return 1;
	}
	
	runRPYOriCalibration(10);
	//runRPYDriftCalibration(10);
	
	while(ros::ok())
	{
		xsens_node::IMUData msg;
		
		updateIMUData();
		
		mImuDriver->accel >> msg.accel;
		mImuDriver->gyro >> msg.gyro;
		mImuDriver->mag >> msg.mag;
		
		*drift_comp_total += *drift_comp;// * *sampleTime;
		
		//ROS_INFO("Offset x %f y %f z %f", ori_comp_total->getX(), ori_comp_total->getY(), ori_comp_total->getZ());
		
		mImuDriver->ori += *drift_comp_total;
		
		tf::Vector3 temp;
		mImuDriver->ori >> temp;
		temp += *ori_comp;
		
		temp >> msg.ori;
		
		//mImuDriver->ori >> msg.ori;
		
		imu_pub.publish(msg);
		ros::spinOnce();
		ros::Rate(110).sleep();
	}
	
	delete mImuDriver;
	delete ori_comp;
	delete drift_comp;
	delete drift_comp_total;
	delete ori_data_cache;
	
	return 0;
}