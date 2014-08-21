#ifndef rgbd_al_module_h_
#define rgbd_al_module_h_

#include "ed/types.h"
#include "ed/rgbd_data.h"

#include <tue/config/configuration.h>

#include <ros/node_handle.h>
#include <visualization_msgs/Marker.h>
#include <ros/publisher.h>

#include <tue/profiling/profiler.h>
#include <tue/profiling/ros/profile_publisher.h>

namespace ed
{

class RGBDALModule
{

public:

    RGBDALModule(const TYPE& type) :
        type_(type)
    {
        ros::NodeHandle nh("~/" + type_);
        vis_marker_pub_ = nh.advertise<visualization_msgs::Marker>("vis_markers",0);

        // Initialize profiler
        profiler_.setName(type_);
        pub_profile_.initialize(profiler_);
    }

    virtual void process(const RGBDData& rgbd_data,
                         PointCloudMaskPtr& not_associated_mask,
                         std::map<UUID, EntityConstPtr>& entities) = 0;

    virtual void configure(tue::Configuration config) = 0;

    const TYPE& getType() { return type_; }

protected:
    TYPE type_;
    ros::Publisher vis_marker_pub_;
    bool visualize_;

    tue::Profiler profiler_;
    tue::ProfilePublisher pub_profile_;

};

}

#endif
