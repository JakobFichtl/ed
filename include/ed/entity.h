#ifndef entity_h_
#define entity_h_

#include "ed/types.h"

#include <boost/circular_buffer.hpp>
#include <ros/ros.h>

namespace ed
{

class Entity
{

public:
    Entity(const UUID& id = generateID(), const TYPE& type = "", const unsigned int& measurement_buffer_size = 5, double creation_time = ros::Time::now().toSec());
    ~Entity();

    static UUID generateID();
    const UUID& id() const { return id_; }

    const TYPE& type() const { return type_; }
    void setType(const TYPE& type) { type_ = type; }

    void measurements(std::vector<MeasurementConstPtr>& measurements, double min_timestamp = 0) const;
    void measurements(std::vector<MeasurementConstPtr>& measurements, unsigned int num) const;
    MeasurementConstPtr lastMeasurement() const;
    unsigned int measurementSeq() const { return measurements_seq_; }
    MeasurementConstPtr bestMeasurement() const { return best_measurement_; }

    void addMeasurement(MeasurementConstPtr measurement);

    inline geo::ShapeConstPtr shape() const { return shape_; }
    void setShape(const geo::ShapeConstPtr& shape);

    inline int shapeRevision() const{ return shape_ ? shape_revision_ : 0; }

    inline const ConvexHull2D& convexHull() const { return convex_hull_; }
    void setConvexHull(const ConvexHull2D& convex_hull) { convex_hull_ = convex_hull; }

    inline const geo::Pose3D& pose() const { return pose_; }
    inline void setPose(const geo::Pose3D& pose) { pose_ = pose; }

    //! For debugging purposes
//    bool in_frustrum;
//    bool object_in_front;

    inline double creationTime() const { return creation_time_; }

private:
    UUID id_;

    TYPE type_;

    boost::circular_buffer<MeasurementConstPtr> measurements_;
    MeasurementConstPtr best_measurement_;
    unsigned int measurements_seq_;

    geo::ShapeConstPtr shape_;
    int shape_revision_;
    ConvexHull2D convex_hull_;

    geo::Pose3D pose_;
    geo::Pose3D velocity_;

    void updateConvexHull();

    double creation_time_;

};

}

#endif
