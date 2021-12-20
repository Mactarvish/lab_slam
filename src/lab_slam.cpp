//
// Created by jin on 2021/6/1.
//

#include "lab_slam.h"

LabSLAM::LabSLAM() {
    ros::NodeHandle nh("~");
    cloud_msg_sub_ = nh.subscribe(topic_name_, 5, &LabSLAM::msgCallback, this);
}

void LabSLAM::msgCallback(const sensor_msgs::PointCloud2::ConstPtr& msg) {
    LOG(INFO) << "Current msg timestamp: " << std::fixed << std::setprecision(6) << msg->header.stamp.toSec();
//    // debug
//    {
//        static int index = 0;
//        static sensor_msgs::PointCloud2::Ptr tmp_msg(new sensor_msgs::PointCloud2);
//        if(index == 0){
//            *tmp_msg = *msg;
//        }
//        tmp_msg->header.stamp = ros::Time(tmp_msg->header.stamp.toSec() + 0.1 * index++);
//    }
    velodyne_msg_mutex_.lock();
    velodyne_msgs_.emplace_back(msg);
//    velodyne_msgs_.emplace_back(tmp_msg);
    velodyne_msg_mutex_.unlock();
    msg_condit_var_.notify_all();
    LOG(INFO) << "Current cloud size: " << velodyne_msgs_.size();
}

void LabSLAM::preprocessWork() {
    while(1){
//        if(velodyne_msgs_.empty()){
//            sleep(0.01);
//            LOG(INFO) << "Sleep";
//            continue;
//        }
//        velodyne_msg_mutex_.lock();
//        sensor_msgs::PointCloud2::ConstPtr cloud = velodyne_msgs_.front();
//        velodyne_msgs_.pop_front();
//        velodyne_msg_mutex_.unlock();
        sensor_msgs::PointCloud2::ConstPtr cloud(new sensor_msgs::PointCloud2);
        std::unique_lock<std::mutex> msg_unique_lock(velodyne_msg_mutex_);
        // 以下，通过函数体返回了想要的数据cloud
        msg_condit_var_.wait(msg_unique_lock, [this, &cloud]()->bool{if(velodyne_msgs_.empty()) {return false;} cloud = velodyne_msgs_.front(); velodyne_msgs_.pop_front(); return true;});
        msg_unique_lock.unlock();
        DataGroupPtr data_group(new DataGroup);
        Timer t("preprocess work");
        pre_processor_.work(cloud, data_group);
        t.end();
        data_mutex_.lock();
        data_.push_back(data_group);
//        LOG(INFO) << "Data group size after preprocess: " << data_.size();
        data_mutex_.unlock();
        data_condit_var_.notify_all();
    }
}

void LabSLAM::lidarOdoWork() {
    while(1){
//        if(data_.empty()){
//           sleep(0.01);
//           continue;
//        }
//        data_mutex_.lock();
//        auto data_group = std::move(data_.front());
//        data_.pop_front();
//        data_mutex_.unlock();
        DataGroupPtr data_group(new DataGroup);
        std::unique_lock<std::mutex> data_unique_lock(data_mutex_);
        data_condit_var_.wait(data_unique_lock, [this, &data_group](){if(data_.empty()) return false; data_group = data_.front(); data_.pop_front(); return true;});
        data_unique_lock.unlock();
        lidar_odo_.work(data_group);
    }
}

int main(int argc, char** argv){
    google::InitGoogleLogging("LabSLAM");
    google::SetLogDestination(google::INFO, "/home/jin/Documents/lab_slam_ws/src/lab_slam/log/my_log_");
    google::SetStderrLogging(google::INFO);
//    FLAGS_alsologtostderr = true;
//    FLAGS_colorlogtostderr = true;
//    google::LogToStderr();
    ros::init(argc, argv, "LabSLAM");
    LabSLAM lab_slam;
    std::thread pre_process(&LabSLAM::preprocessWork, &lab_slam);
    std::thread lidar_odo(&LabSLAM::lidarOdoWork, &lab_slam);
    ros::spin();
    return 0;
}