#include <base_libs/macros.h>
#include <base_libs/multi_subscriber.h>
#include <ros/rate.h>
#include <std_msgs/String.h>
#include <geometry_msgs/Point.h>

class TestMultiSubscriberNode
{
public:
	ros::MultiSubscriber<> multi_sub_;
	ros::Rate loop_rate_;
	
	TestMultiSubscriberNode( ros::NodeHandle & nh )
	:
		multi_sub_(),
		loop_rate_( 10 )
	{
		multi_sub_.addSubscriber(
				nh,
				"string",
				&TestMultiSubscriberNode::stringCB, this );
		
		multi_sub_.addSubscriber(
				nh,
				"point",
				&TestMultiSubscriberNode::pointCB, this );
	}
	
	BASE_LIBS_DECLARE_STANDARD_CALLBACK( stringCB, std_msgs::String )
	{
		printf( "Got string: %s\n", msg->data.c_str() );
	}
	
	BASE_LIBS_DECLARE_STANDARD_CALLBACK( pointCB, geometry_msgs::Point )
	{
		printf( "Got point: [%f %f %f] %d\n", msg->x, msg->y, msg->z );
	}
	
	void spinOnce()
	{
		//
	}
	
	void spin()
	{
		while( ros::ok() )
		{
			spinOnce();
			ros::spinOnce();
			loop_rate_.sleep();
		}
	}
};

BASE_LIBS_INST_NODE( TestMultiSubscriberNode, "test_multi_subscriber_node" )
