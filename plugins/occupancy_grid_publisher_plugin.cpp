#include "occupancy_grid_publisher_plugin.h"

//#include <ros/node_handle.h>
#include <nav_msgs/OccupancyGrid.h>

#include <geolib/ros/msg_conversions.h>
#include <geolib/Shape.h>

#include <ed/world_model.h>
#include <ed/entity.h>
#include <ed/measurement.h>

#include <tue/config/reader.h>

// ----------------------------------------------------------------------------------------------------

void OccupancyGridPublisherPlugin::configure(tue::Configuration config)
{
    config.value("frequency", frequency_);

    config.value("resolution", res_);

    std::string old_topic = topic_;
    config.value("topic", topic_);
    config.value("frame_id", frame_id_);

    config.value("specifier", specifier_, tue::OPTIONAL);
    config.value("sim_time", sim_time_, tue::OPTIONAL);

    //! Re-initialize if topic has changed
    if (old_topic != "" && topic_ != old_topic)
    {
        initialize();
    }

    ros::NodeHandle nh("~");

    ros::AdvertiseServiceOptions opt_srv_get_goal_area =
            ros::AdvertiseServiceOptions::create<ed::GetGoalArea>(
                "nav/get_goal_area", boost::bind(&OccupancyGridPublisherPlugin::srvGetGoalArea, this, _1, _2),
                ros::VoidPtr(), &cb_queue_);

    srv_get_goal_area_ = nh.advertiseService(opt_srv_get_goal_area);
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
    // Check for services
    world_ = &world;
    cb_queue_.callAvailable();

    std::vector<ed::EntityConstPtr> entities_to_be_projected;
    if (getMapData(world, entities_to_be_projected))
    {
        cv::Mat map = cv::Mat::zeros(height_, width_, CV_8U);

        for(std::vector<ed::EntityConstPtr>::const_iterator it = entities_to_be_projected.begin(); it != entities_to_be_projected.end(); ++it)
            updateMap(*it, map);

        publishMapMsg(map);
    }
    else
    {
        std::cout << "Error getting map data:" << std::endl;
        std::cout << "width: " << width_ << std::endl;
        std::cout << "height: " << height_ << std::endl;
        std::cout << "resolution: " << res_ << std::endl;
    }
}

// ----------------------------------------------------------------------------------------------------

bool OccupancyGridPublisherPlugin::getMapData(const ed::WorldModel& world, std::vector<ed::EntityConstPtr>& entities_to_be_projected)
{
    geo::Vector3 min(1e6,1e6,0);
    geo::Vector3 max(-1e6,-1e6,0);

    for(ed::WorldModel::const_iterator it = world.begin(); it != world.end(); ++it)
    {
        const ed::EntityConstPtr& e = *it;

        //! Push back the entity
        entities_to_be_projected.push_back(e);

        //! Update the map bounds
        geo::ShapeConstPtr shape = e->shape();
        if (shape)  // Do shape
        {
            const std::vector<geo::Vector3>& vertices = shape->getMesh().getPoints();
            for(std::vector<geo::Vector3>::const_iterator it = vertices.begin(); it != vertices.end(); ++it) {

                geo::Vector3 p1w = e->pose() * (*it);

                // Filter the ground
                if (p1w.getZ() > 0.05001)
                {
                    min.x = std::min(p1w.x, min.x);
                    max.x = std::max(p1w.x, max.x);

                    min.y = std::min(p1w.y, min.y);
                    max.y = std::max(p1w.y, max.y);
                }
            }
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

    return (width_ > 0 && height_ > 0 && width_*height_ < 100000000);
}

// ----------------------------------------------------------------------------------------------------

void OccupancyGridPublisherPlugin::updateMap(const ed::EntityConstPtr& e, cv::Mat& map)
{
    int value = 100;

    //! Check object persistence time
    double persistence_time;
    if (tue::config::Reader(e->data()).value("object_persistence", persistence_time, tue::config::OPTIONAL) && e->lastMeasurement())
    {
        std::cout << e->id() << std::endl;
        if ((ros::Time::now().toSec() - e->lastMeasurement()->timestamp()) > persistence_time)
            value = 99;
    }

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
                    cv::line(map, p1, p2, value);
                    cv::line(map, p1, p3, value);
                    cv::line(map, p2, p3, value);
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
                cv::line(map, p1, p2, value);

            // Velocity
            ed::MeasurementConstPtr m = e->lastMeasurement();
            if (m && (ros::Time::now().toSec() - m->timestamp()) < sim_time_)
            {
                if (sim_time_ > 0 && e->velocity().t.length2() > 1e-6)
                {
                    p1w += sim_time_ * e->velocity().t;
                    p2w += sim_time_ * e->velocity().t;
                    if ( worldToMap(p1w.x, p1w.y, p1.x, p1.y) && worldToMap(p2w.x, p2w.y, p2.x, p2.y) )
                        cv::line(map, p1, p2, 100);
                }
            }
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

bool OccupancyGridPublisherPlugin::srvGetGoalArea(const ed::GetGoalArea::Request& req, ed::GetGoalArea::Response& res)
{
    ed::EntityConstPtr e = world_->getEntity(req.entity_id);

    if (!e)
    {
        res.error_msg = "No such entity: '" + req.entity_id + "'.";
        return true;
    }

    const tue::config::DataConstPointer& data = e->data();
    if (data.empty())
    {
        res.error_msg = "Entity '" + e->id().str() + "': does not have an 'areas' property.";
        return true;
    }

    tue::config::Reader r(data);
    if (!r.readArray("areas"))
    {
        res.error_msg = "Entity '" + e->id().str() + "': does not have an 'areas' property.";
        return true;
    }

    while(r.nextArrayItem())
    {
        std::string name;
        if (r.value("name", name) && name == req.area_name)
        {
            if (!r.readArray("shape", tue::config::REQUIRED))
            {
                res.error_msg = "Entity '" + e->id().str() + "': area '" + req.area_name + "' does not have 'shape' property.";
                return true;
            }

            // Construct position constraint from shape
            std::string& pc = res.position_constraint;

            while(r.nextArrayItem())
            {
                if (r.readGroup("box"))
                {
                    geo::Vector3 min, max;

                    if (r.readGroup("min"))
                    {
                        r.value("x", min.x);
                        r.value("y", min.y);
                        r.value("z", min.z);
                        r.endGroup();
                    }

                    if (r.readGroup("max"))
                    {
                        r.value("x", max.x);
                        r.value("y", max.y);
                        r.value("z", max.z);
                        r.endGroup();
                    }

                    std::cout << "Box: " << min << " - " << max << std::endl;

                    // Add to position constraint here:

                    // ....

//                    By Rokus from navigate_to_observe.py

//                    x = e.pose.position.x
//                    y = e.pose.position.y

//                    ch.append(ch[0])

//                    pci = ""

//                    for i in xrange(len(ch) - 1):
//                        dx = ch[i+1].x - ch[i].x
//                        dy = ch[i+1].y - ch[i].y

//                        length = (dx * dx + dy * dy)**.5

//                        xs = ch[i].x + (dy/length)*self.radius
//                        ys = ch[i].y - (dx/length)*self.radius

//                        if i != 0:
//                            pci = pci + ' and '

//                        pci = pci + "-(x-%f)*%f+(y-%f)*%f > 0.0 "%(xs, dy, ys, dx)

//                    pc = PositionConstraint(constraint=pci, frame="/map")
//                    oc = OrientationConstraint(look_at=Point(x, y, 0.0), frame="/map")

                    pc += "Something";

                    r.endGroup();
                }
            }

            r.endArray();

            return true;
        }
    }

    r.endArray();

    res.error_msg = "Entity '" + e->id().str() + "': area '" + req.area_name + "' does not exist";
    return true;
}

ED_REGISTER_PLUGIN(OccupancyGridPublisherPlugin)



