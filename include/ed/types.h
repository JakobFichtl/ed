#ifndef ED_TYPES_H_
#define ED_TYPES_H_

#include <rgbd/types.h>
#include <geolib/datatypes.h>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>

#include <opencv/cv.h>

#include "ed/mask.h"

namespace ed
{

typedef unsigned int Idx;
static const unsigned int INVALID_IDX = -1;

class Measurement;
typedef boost::shared_ptr<Measurement> MeasurementPtr;
typedef boost::shared_ptr<const Measurement> MeasurementConstPtr;

class Entity;
typedef boost::shared_ptr<Entity> EntityPtr;
typedef boost::shared_ptr<const Entity> EntityConstPtr;

class Plugin;
typedef boost::shared_ptr<Plugin> PluginPtr;
typedef boost::shared_ptr<const Plugin> PluginConstPtr;

class WorldModel;
typedef boost::shared_ptr<WorldModel> WorldModelPtr;
typedef boost::shared_ptr<const WorldModel> WorldModelConstPtr;

class UpdateRequest;
typedef boost::shared_ptr<UpdateRequest> UpdateRequestPtr;
typedef boost::shared_ptr<const UpdateRequest> UpdateRequestConstPtr;

class PluginContainer;
typedef boost::shared_ptr<PluginContainer> PluginContainerPtr;
typedef boost::shared_ptr<const PluginContainer> PluginContainerConstPtr;

class SensorModule;
typedef boost::shared_ptr<SensorModule> SensorModulePtr;
typedef boost::shared_ptr<const SensorModule> SensorModuleConstPtr;

class RGBDALModule;
typedef boost::shared_ptr<RGBDALModule> RGBDALModulePtr;
typedef boost::shared_ptr<const RGBDALModule> RGBDALModuleConstPtr;

class RGBDSegModule;
typedef boost::shared_ptr<RGBDSegModule> RGBDSegModulePtr;
typedef boost::shared_ptr<const RGBDSegModule> RGBDSegModuleConstPtr;

class PerceptionModule;
typedef boost::shared_ptr<PerceptionModule> PerceptionModulePtr;
typedef boost::shared_ptr<const PerceptionModule> PerceptionModuleConstPtr;

class Relation;
typedef boost::shared_ptr<Relation> RelationPtr;
typedef boost::shared_ptr<const Relation> RelationConstPtr;

class UUID
{

public:

    UUID() : idx(INVALID_IDX) {}
    UUID(const char* s) : id_(s), idx(INVALID_IDX) {}
    UUID(const std::string& s) : id_(s), idx(INVALID_IDX) {}

    inline bool operator<(const UUID& rhs) const { return id_ < rhs.id_; }

    inline bool operator==(const UUID& rhs) const { return id_ == rhs.id_; }

    inline bool operator!=(const UUID& rhs) const { return id_ != rhs.id_; }

    inline const char* c_str() const { return id_.c_str(); }

    inline const std::string& str() const { return id_; }

    friend std::ostream& operator<< (std::ostream& out, const UUID& d)
    {
        out << d.id_;
        return out;
    }

private:

    std::string id_;

public:

    mutable Idx idx;

};

//typedef std::string UUID;
typedef std::string TYPE;

typedef std::vector< std::vector<cv::Point2i> > IndexMap;

struct ConvexHull2D {
    ConvexHull2D() : center_point(geo::Vector3(0,0,0)) {}
    pcl::PointCloud<pcl::PointXYZ> chull; // Convex hull point w.r.t. center
    double min_z, max_z; // min and max z of convex hull
    geo::Vector3 center_point; // Center of the convex hull
};

struct ConvexHull2DWithIndices {
    std::vector<cv::Point2i> indices;
    ConvexHull2D convex_hull_2d;
};

}

#endif
