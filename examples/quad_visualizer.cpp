#include "global_init.h"
#include "marker.pb.h"
#include "node_handle.h"
#include "geometry_msgs.pb.h"
#include <deque>
#include <vector>
#include <iostream>
#include <memory>
#include <Eigen/Dense>
#include <cmath>

class QuadVisualizer {
public:
    void run() {
        auto& sys = SystemManager::instance();
        sys.init("quad_visualizer");
        nh_ = std::make_shared<NodeHandle>();

        marker_pub_ = nh_->advertise<visualization_msgs::MarkerArray>("quad_marker_array");
        short_path_pub_ = nh_->advertise<visualization_msgs::Marker>("quad_path_short");
        incremental_path_pub_ = nh_->advertise<visualization_msgs::Marker>("quad_path_incremental");

        // 修复Subscriber类型
        odom_sub_ = nh_->subscribe<geometry_msgs::Odometry>(
            "quad_odometry",
            10,
            [this](const std::shared_ptr<geometry_msgs::Odometry>& odom) {
                this->odomCallback(odom);
            }
        );

        std::cout << "Quad Visualizer Running..." << std::endl;
        sys.spin();
    }

private:
    std::shared_ptr<NodeHandle> nh_;
    std::shared_ptr<Publisher<visualization_msgs::MarkerArray>> marker_pub_;
    std::shared_ptr<Publisher<visualization_msgs::Marker>> short_path_pub_;
    std::shared_ptr<Publisher<visualization_msgs::Marker>> incremental_path_pub_;
    std::shared_ptr<Subscriber> odom_sub_;  // ← 去掉模板参数

    std::deque<geometry_msgs::Point> short_path_points_;
    std::vector<geometry_msgs::Point> incremental_path_points_;
    geometry_msgs::Point last_point_;
    bool has_last_point_ = false;


    void odomCallback(const std::shared_ptr<geometry_msgs::Odometry>& odom) {
        double x = odom->pose().position().x();
        double y = odom->pose().position().y();
        double z = odom->pose().position().z();
        double qx = odom->pose().orientation().x();
        double qy = odom->pose().orientation().y();
        double qz = odom->pose().orientation().z();
        double qw = odom->pose().orientation().w();

        // 更新轨迹
        geometry_msgs::Point p;
        p.set_x(x); p.set_y(y); p.set_z(z);
        short_path_points_.push_back(p);
        while (short_path_points_.size() > 150) short_path_points_.pop_front();

        incremental_path_points_.push_back(p);

        // 发布MarkerArray（机身Cube + 四臂 + 螺旋桨）
        visualization_msgs::MarkerArray marker_array;
        int id = 0;

        // --- 机身 ---
        visualization_msgs::Marker body;
        body.set_ns("quadrotor");
        body.set_id(id++);
        body.set_type(visualization_msgs::MarkerType::CUBE);
        body.set_action(visualization_msgs::MarkerAction::ADD);
        body.mutable_scale()->set_x(0.3);
        body.mutable_scale()->set_y(0.3);
        body.mutable_scale()->set_z(0.1);
        body.mutable_pose()->mutable_position()->set_x(x);
        body.mutable_pose()->mutable_position()->set_y(y);
        body.mutable_pose()->mutable_position()->set_z(z);
        body.mutable_pose()->mutable_orientation()->set_x(qx);
        body.mutable_pose()->mutable_orientation()->set_y(qy);
        body.mutable_pose()->mutable_orientation()->set_z(qz);
        body.mutable_pose()->mutable_orientation()->set_w(qw);
        body.mutable_color()->set_r(0.2);
        body.mutable_color()->set_g(0.2);
        body.mutable_color()->set_b(0.8);
        body.mutable_color()->set_a(1.0);
        *marker_array.add_markers() = body;

        // --- 四个臂 ---
        double arm_length = 0.6;
        double arm_radius = 0.03;
        double offsets[4][3] = {
            {arm_length/2, 0.0, 0.02},
            {-arm_length/2, 0.0, 0.02},
            {0.0, arm_length/2, 0.02},
            {0.0, -arm_length/2, 0.02}
        };
        Eigen::Matrix3d R;
        R = Eigen::Quaterniond(qw, qx, qy, qz).toRotationMatrix();
        for (int i = 0; i < 4; i++) {
            visualization_msgs::Marker arm;
            arm.set_ns("quadrotor");
            arm.set_id(id++);
            arm.set_type(visualization_msgs::MarkerType::CYLINDER);
            arm.set_action(visualization_msgs::MarkerAction::ADD);
            arm.mutable_scale()->set_x(arm_radius);
            arm.mutable_scale()->set_y(arm_radius);
            arm.mutable_scale()->set_z(0.02);
            Eigen::Vector3d offset(offsets[i][0], offsets[i][1], offsets[i][2]);
            Eigen::Vector3d pos = R * offset + Eigen::Vector3d(x, y, z);
            arm.mutable_pose()->mutable_position()->set_x(pos.x());
            arm.mutable_pose()->mutable_position()->set_y(pos.y());
            arm.mutable_pose()->mutable_position()->set_z(pos.z());
            arm.mutable_pose()->mutable_orientation()->set_x(qx);
            arm.mutable_pose()->mutable_orientation()->set_y(qy);
            arm.mutable_pose()->mutable_orientation()->set_z(qz);
            arm.mutable_pose()->mutable_orientation()->set_w(qw);
            arm.mutable_color()->set_r(0.8);
            arm.mutable_color()->set_g(0.2);
            arm.mutable_color()->set_b(0.2);
            arm.mutable_color()->set_a(1.0);
            *marker_array.add_markers() = arm;
        }

        // --- 四个螺旋桨 ---
        double prop_radius = 0.3;
        double prop_thick = 0.02;
        for (int i = 0; i < 4; i++) {
            visualization_msgs::Marker prop;
            prop.set_ns("quadrotor");
            prop.set_id(id++);
            prop.set_type(visualization_msgs::MarkerType::CYLINDER);
            prop.set_action(visualization_msgs::MarkerAction::ADD);
            prop.mutable_scale()->set_x(prop_radius);
            prop.mutable_scale()->set_y(prop_radius);
            prop.mutable_scale()->set_z(prop_thick);
            Eigen::Vector3d offset(offsets[i][0], offsets[i][1], offsets[i][2]+0.02);
            Eigen::Vector3d pos = R * offset + Eigen::Vector3d(x, y, z);
            prop.mutable_pose()->mutable_position()->set_x(pos.x());
            prop.mutable_pose()->mutable_position()->set_y(pos.y());
            prop.mutable_pose()->mutable_position()->set_z(pos.z());
            prop.mutable_pose()->mutable_orientation()->set_x(qx);
            prop.mutable_pose()->mutable_orientation()->set_y(qy);
            prop.mutable_pose()->mutable_orientation()->set_z(qz);
            prop.mutable_pose()->mutable_orientation()->set_w(qw);
            prop.mutable_color()->set_r(0.2);
            prop.mutable_color()->set_g(0.8);
            prop.mutable_color()->set_b(0.2);
            prop.mutable_color()->set_a(1.0);
            *marker_array.add_markers() = prop;
        }

        marker_pub_->publish(marker_array);

        // --- 短期轨迹 ---
        visualization_msgs::Marker line_short;
        line_short.set_ns("quad_path_short");
        line_short.set_id(0);
        line_short.set_type(visualization_msgs::MarkerType::LINE_STRIP);
        line_short.set_action(visualization_msgs::MarkerAction::ADD);
        line_short.mutable_color()->set_r(1.0);
        line_short.mutable_color()->set_g(0.0);
        line_short.mutable_color()->set_b(0.0);
        line_short.mutable_color()->set_a(1.0);
        line_short.mutable_scale()->set_x(0.35);
        line_short.set_lifetime(50);
        for (const auto& pt : short_path_points_) {
            geometry_msgs::Point* new_pt = line_short.add_points();
            new_pt->set_x(pt.x());
            new_pt->set_y(pt.y());
            new_pt->set_z(pt.z());
        }
        short_path_pub_->publish(line_short);

        // --- 增量轨迹 ---
        if (has_last_point_) {
            visualization_msgs::Marker line_inc;
            line_inc.set_ns("quad_path_incremental");
            line_inc.set_id(1);
            line_inc.set_type(visualization_msgs::MarkerType::LINE_STRIP);
            line_inc.set_action(visualization_msgs::MarkerAction::ADD);
            line_inc.set_lifetime(-1); // 永久保留
            line_inc.mutable_color()->set_r(0.0);
            line_inc.mutable_color()->set_g(1.0);
            line_inc.mutable_color()->set_b(0.0);
            line_inc.mutable_color()->set_a(1.0);
            line_inc.mutable_scale()->set_x(0.15);

            // 只加两个点 (last_point, current_point)
            geometry_msgs::Point* p1 = line_inc.add_points();
            p1->set_x(last_point_.x());
            p1->set_y(last_point_.y());
            p1->set_z(last_point_.z());

            geometry_msgs::Point* p2 = line_inc.add_points();
            p2->set_x(p.x());
            p2->set_y(p.y());
            p2->set_z(p.z());

            incremental_path_pub_->publish(line_inc);
        }

        // 更新 last_point
        last_point_ = p;
        has_last_point_ = true;
    }
};

int main() {
    try {
        QuadVisualizer vis;
        vis.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
