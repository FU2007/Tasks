#include "geometry_msgs/msg/twist.hpp"
#include "rclcpp/rclcpp.hpp"
#include "turtlesim/msg/pose.hpp"

class TurtleControl : public rclcpp::Node
{
    private:
        rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr velocity_publisher_;
        rclcpp::Subscription<turtlesim::msg::Pose>::SharedPtr pose_subscriber_;
        double target_x_ = {1.0};
        double target_y_ = {1.0};
        double k_{1.0};
        double max_speed_{3.0};

        void pose_received(const turtlesim::msg::Pose::SharedPtr pose)
        {
            auto twist_msg = geometry_msgs::msg::Twist();
            double current_x = pose->x;
            double current_y = pose->y;
            RCLCPP_INFO(this->get_logger(), "Current Position: x=%.2f, y=%.2f", current_x, current_y);

            double error_x = target_x_ - current_x;
            double error_y = target_y_ - current_y;
            double distance = std::sqrt(error_x * error_x + error_y * error_y);
            double angle_to_target = std::atan2(error_y, error_x);
            if (distance > 0.1)
            {
                if(fabs(angle_to_target - pose->theta) > 0.2)
                {
                     twist_msg.angular.z = fabs(angle_to_target - pose->theta) ;
                }
                else
                {
                    twist_msg.linear.x = std::min(k_ * distance, max_speed_);
                }
            }
            else
            {
                twist_msg.linear.x = 0.0;
                twist_msg.angular.z = 0.0;
                RCLCPP_INFO(this->get_logger(), "Target reached!");
            }
            velocity_publisher_->publish(twist_msg);
           
        }
    public:
        TurtleControl() : Node("turtle_control")
        {
            // Create a publisher for the turtle's velocity
            velocity_publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("turtle1/cmd_vel", 10);

            // Create a subscriber for the turtle's pose
            pose_subscriber_ = this->create_subscription<turtlesim::msg::Pose>(
                "turtle1/pose", 10, std::bind(&TurtleControl::pose_received, this, std::placeholders::_1));
        }

};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TurtleControl>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}