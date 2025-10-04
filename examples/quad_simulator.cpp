#include "global_init.h"
#include "node_handle.h"
#include "geometry_msgs.pb.h"
#include <thread>
#include <chrono>
#include <memory>
#include <iostream>
#include <Eigen/Dense>
#include <cmath>

using namespace std::chrono_literals;

struct QuadState {
    double x, y, z;
    double qx, qy, qz, qw;
    double vx, vy, vz;
    double wx, wy, wz;
};

class QuadSimulator {
public:
    QuadSimulator() : counter_(0) {}

    void run() {
        auto& sys = SystemManager::instance();
        sys.init("quad_simulator");
        std::this_thread::sleep_for(200ms);

        nh_ = std::make_shared<NodeHandle>();
        odom_pub_ = nh_->advertise<geometry_msgs::Odometry>("quad_odometry");

        timer_ = nh_->createTimer(
            0.02,
            std::bind(&QuadSimulator::timerCallback, this, std::placeholders::_1),
            false
        );

        std::cout << "Quad Simulator Running..." << std::endl;
        sys.spin();
    }

private:
    int counter_;
    std::shared_ptr<NodeHandle> nh_;
    std::shared_ptr<Publisher<geometry_msgs::Odometry>> odom_pub_;
    std::shared_ptr<Timer> timer_;

    void timerCallback(const TimerEvent& event) {
        double dt = 0.02;
        double radius = 2.0;
        double speed = 2.0;
        double z_amp = 0.2;
        double z_freq = 0.5;

        double t = counter_ * dt;

        // 位置
        double x = radius * cos(speed * t);
        double y = radius * sin(speed * t);
        double z = 0.5 + z_amp * sin(z_freq * t);

        // 速度
        double vx = -speed * radius * sin(speed * t);
        double vy =  speed * radius * cos(speed * t);
        double vz =  z_amp * z_freq * cos(z_freq * t);

        // 加速度
        double ax = -speed*speed*radius*cos(speed*t);
        double ay = -speed*speed*radius*sin(speed*t);
        double az = -z_amp*z_freq*z_freq*sin(z_freq*t);

        // 计算四元数
        QuadState state = computeFlatness(x, y, z, vx, vy, vz, ax, ay, az);

        // 发布Odometry
        geometry_msgs::Odometry odom_msg;
        odom_msg.mutable_pose()->mutable_position()->set_x(state.x);
        odom_msg.mutable_pose()->mutable_position()->set_y(state.y);
        odom_msg.mutable_pose()->mutable_position()->set_z(state.z);
        odom_msg.mutable_pose()->mutable_orientation()->set_x(state.qx);
        odom_msg.mutable_pose()->mutable_orientation()->set_y(state.qy);
        odom_msg.mutable_pose()->mutable_orientation()->set_z(state.qz);
        odom_msg.mutable_pose()->mutable_orientation()->set_w(state.qw);

        odom_msg.mutable_linear_velocity()->set_x(state.vx);
        odom_msg.mutable_linear_velocity()->set_y(state.vy);
        odom_msg.mutable_linear_velocity()->set_z(state.vz);

        odom_msg.mutable_angular_velocity()->set_x(state.wx);
        odom_msg.mutable_angular_velocity()->set_y(state.wy);
        odom_msg.mutable_angular_velocity()->set_z(state.wz);

        odom_pub_->publish(odom_msg);

        counter_++;
    }

    QuadState computeFlatness(double x, double y, double z,
                              double vx, double vy, double vz,
                              double ax, double ay, double az) {
        QuadState state;
        state.x = x; state.y = y; state.z = z;
        state.vx = vx; state.vy = vy; state.vz = vz;

        Eigen::Vector3d acc(ax, ay, az + 9.81);
        double yaw = std::atan2(vy, vx);
        Eigen::Vector3d Z_b = acc.normalized();
        Eigen::Vector3d X_c(cos(yaw), sin(yaw), 0);
        Eigen::Vector3d Y_b = Z_b.cross(X_c).normalized();
        Eigen::Vector3d X_b = Y_b.cross(Z_b);

        Eigen::Matrix3d R;
        R.col(0) = X_b; R.col(1) = Y_b; R.col(2) = Z_b;

        Eigen::Quaterniond quat(R);
        state.qx = quat.x(); state.qy = quat.y();
        state.qz = quat.z(); state.qw = quat.w();

        state.wx = 0; state.wy = 0; state.wz = 0;

        return state;
    }
};

int main() {
    try {
        QuadSimulator sim;
        sim.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
