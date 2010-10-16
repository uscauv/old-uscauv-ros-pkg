#include <base_image_proc/base_image_proc.h>

typedef BaseImageProcSettings::_DefaultReconfigureType _ConfigType;

class Demo1 : public BaseImageProc<_ConfigType>
{
	Demo1( ros__NodeHandle & nh ) :  BaseImageProc<base_image_proc::EmptyConfig>(nh)
	{
		//
	}

	virtual cv::Mat processImage( IplImage * ipl_img)
	{
		cv_img_ = cv::Mat( ipl_img );
		
		//cv::ellipse(Mat& img, const RotatedRect& box, const Scalar& 			color, int thickness=1, int lineType=8)
		cv::line(cv_img_, cv::Point(30,0), cv::Point(50,50), cv::Scalar(150,0,175));

		return cv_img_;
	}

};

int main( int argc, char **argv)
{
	ros::init( argc, argv, "demo1_dhruv" );
	ros::NodeHandle nh;

	Demo1 demo1( nh );
	demo1.spin();

	return 0;
}

