#include "occupancy_grid_publisher_plugin.h"

//#include <ros/node_handle.h>
#include <nav_msgs/OccupancyGrid.h>

#include <geolib/ros/msg_conversions.h>
#include <geolib/Shape.h>

#include <ed/world_model.h>
#include <ed/entity.h>

// ----------------------------------------------------------------------------------------------------

void OccupancyGridPublisherPlugin::configure(tue::Configuration config)
{
    config.value("frequency", frequency_);

    config.value("resolution", res_);

    std::string old_topic = topic_;
    config.value("topic", topic_);
    config.value("frame_id", frame_id_);

    config.value("specifier", specifier_, tue::OPTIONAL);

    //! Re-initialize if topic has changed
    if (old_topic != "" && topic_ != old_topic)
    {
        initialize();
    }
}

// ----------------------------------------------------------------------------------------------------

void OccupancyGridPublisherPlugin::initialize()
{
    ros::NodeHandle nh;
    map_pub_ = nh.advertise<nav_msgs::OccupancyGrid>(topic_, 0, false);
}

// ----------------------------------------------------------------------------------------------------

void OccupancyGridPublisherPlugin::process(const ed::WorldModel& world, ed::UpdateRequest& req)
{
    std::vector<ed::EntityConstPtr> entities_to_be_projected;
    if (getMapData(world, entities_to_be_projected))
    {
        cv::Mat map = cv::Mat::zeros(height_, width_, CV_8U);

        for(std::vector<ed::EntityConstPtr>::const_iterator it = entities_to_be_projected.begin(); it != entities_to_be_projected.end(); ++it)
            updateMap(*it, map);

        publishMapMsg(map);
    }
}

// ----------------------------------------------------------------------------------------------------

bool OccupancyGridPublisherPlugin::getMapData(const ed::WorldModel& world, std::vector<ed::EntityConstPtr>& entities_to_be_projected)
{
    geo::Vector3 min(1e6,1e6,0);
    geo::Vector3 max(-1e6,-1e6,0);
    for(std::map<ed::UUID, ed::EntityConstPtr>::const_iterator it = world.begin(); it != world.end(); ++it)
    {
        ed::EntityConstPtr e = it->second;

        //! Check the specifier, if set
        if (specifier_ != "")
        {
            double val;
            if (!e->getConfig().value(specifier_, val, tue::OPTIONAL) || !val)
                continue;
        }

        //! Push back the entity
        entities_to_be_projected.push_back(it->second);

        //! Update the map bounds
        geo::ShapeConstPtr shape = e->shape();
        if (shape)  // Do shape
        {
            double r = shape->getMesh().getMaxRadius();

            min.x = std::min(e->pose().getOrigin().x - r, min.x);
            max.x = std::max(e->pose().getOrigin().x + r, max.x);

            min.y = std::min(e->pose().getOrigin().y - r, min.y);
            max.y = std::max(e->pose().getOrigin().y + r, max.y);
        }
    }

    // Bounds fix
    min.x-=1.0;
    min.y-=1.0;

    max.x+=1.0;
    max.y+=1.0;

    //! Set the origin, width and height
    origin_ = min;
    width_ = (max.x - min.x) / res_;
    height_ = (max.y - min.y) / res_;

    return (width_ > 0 && height_ > 0);
}

// ----------------------------------------------------------------------------------------------------

void OccupancyGridPublisherPlugin::updateMap(const ed::EntityConstPtr& e, cv::Mat& map)
{
    geo::ShapeConstPtr shape = e->shape();
    if (shape)  // Do shape
    {
        const std::vector<geo::Triangle>& triangles = shape->getMesh().getTriangles();

        for(std::vector<geo::Triangle>::const_iterator it = triangles.begin(); it != triangles.end(); ++it) {

            geo::Vector3 p1w = e->pose() * it->p1_;
            geo::Vector3 p2w = e->pose() * it->p2_;
            geo::Vector3 p3w = e->pose() * it->p3_;

            // Filter the ground
            if (p1w.getZ() > 0.05001 && p2w.getZ() > 0.050001 && p3w.getZ() > 0.05001) {

                cv::Point2i p1, p2, p3;

                // Check if all points are on the map
                if ( worldToMap(p1w.x, p1w.y, p1.x, p1.y) && worldToMap(p2w.x, p2w.y, p2.x, p2.y) && worldToMap(p3w.x, p3w.y, p3.x, p3.y) ) {
                    cv::line(map, p1, p2, 100);
                    cv::line(map, p1, p3, 100);
                    cv::line(map, p2, p3, 100);
                }
            }
        }
    }
    else // Do convex hull
    {
        const pcl::PointCloud<pcl::PointXYZ>& chull_points = e->convexHull().chull;

        for (unsigned int i = 0; i < chull_points.size(); ++i)
        {

            geo::Vector3 p1w(chull_points.points[i].x, chull_points.points[i].y, 0);
            geo::Vector3 p2w;
            if (i == chull_points.size() - 1)
                p2w = geo::Vector3(chull_points.points[0].x, chull_points.points[0].y, 0);
            else
                p2w = geo::Vector3(chull_points.points[i+1].x, chull_points.points[i+1].y, 0);

            // Check if all points are on the map
            cv::Point2i p1, p2;
            if ( worldToMap(p1w.x, p1w.y, p1.x, p1.y) && worldToMap(p2w.x, p2w.y, p2.x, p2.y) )
                cv::line(map, p1, p2, 100);

        }
    }
}

// ----------------------------------------------------------------------------------------------------

void OccupancyGridPublisherPlugin::publishMapMsg(const cv::Mat& map)
{
    nav_msgs::OccupancyGrid map_msg;
    geo::convert(origin_, map_msg.info.origin.position);

    map_msg.info.resolution = res_;
    map_msg.info.width = width_;
    map_msg.info.height = height_;

    map_msg.data.resize(width_ * height_);
    unsigned int i = 0;
    for(int my = 0; my < height_; ++my)
    {
        for(int mx = 0; mx < width_; ++mx)
        {
            unsigned char c = map.at<unsigned char>(my, mx);
            if (c > 0)
                map_msg.data[i] = c;
            else
                map_msg.data[i] = -1;
            ++i;
        }
    }

    map_msg.header.stamp = ros::Time::now();
    map_msg.header.frame_id = frame_id_;
    map_pub_.publish(map_msg);
}

// ----------------------------------------------------------------------------------------------------

ED_REGISTER_PLUGIN(OccupancyGridPublisherPlugin)



