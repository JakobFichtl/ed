#include "ed/models/model_loader.h"

#include "ed/update_request.h"
#include "ed/entity.h"
#include "ed/relations/transform_cache.h"

#include <tue/filesystem/path.h>
#include <ros/package.h>

#include "shape_loader.h"

#include <tue/config/reader.h>
#include <tue/config/writer.h>
#include <tue/config/configuration.h>

#include <sstream>

namespace ed
{

namespace models
{

// ----------------------------------------------------------------------------------------------------

ModelLoader::ModelLoader()
{
}

// ----------------------------------------------------------------------------------------------------

ModelLoader::~ModelLoader()
{
}

// ----------------------------------------------------------------------------------------------------

tue::filesystem::Path getModelPath(const std::string& type)
{
    return tue::filesystem::Path(ros::package::getPath("ed_object_models") + "/models/" + type);
}

// ----------------------------------------------------------------------------------------------------

tue::config::DataConstPointer ModelLoader::loadModelData(const std::string& type, std::stringstream& error)
{
    std::map<std::string, tue::config::DataConstPointer>::iterator it = model_cache_.find(type);
    if (it != model_cache_.end())
    {
        const tue::config::DataConstPointer& data = it->second;
        return data;
    }

    tue::config::DataPointer data;

    tue::filesystem::Path model_path = getModelPath(type);
    if (!model_path.exists())
    {
        error << "ed::models::create() : ERROR loading model '" << type << "'; '" << model_path.string() << "' does not exist." << std::endl;
        return data;
    }

    tue::filesystem::Path model_cfg_path(model_path.string() + "/model.yaml");
    if (!model_cfg_path.exists())
    {
        error << "ed::models::create() : ERROR loading configuration for model '" << type << "'; '" << model_cfg_path.string() << "' file does not exist." << std::endl;
        return data;
    }

    tue::Configuration model_cfg;
    if (!model_cfg.loadFromYAMLFile(model_cfg_path.string()))
    {
        error << "ed::models::create() : ERROR loading configuration for model '" << type << "'; '" << model_cfg_path.string() << "' failed to parse yaml file." << std::endl;
        return data;
    }

    std::string super_type;
    if (model_cfg.value("type", super_type, tue::OPTIONAL))
    {
        tue::config::DataConstPointer super_data = loadModelData(super_type, error);
        tue::config::DataPointer combined_data;
        combined_data.add(super_data);
        combined_data.add(model_cfg.data());

        data = combined_data;
    }
    else
    {
        data = model_cfg.data();
    }

    // If model loads a shape, set model path in shape data
    tue::config::ReaderWriter rw(data);
    if (rw.readGroup("shape"))
    {
        rw.setValue("__model_path__", model_path.string());
        rw.endGroup();
    }

    // Store data in cache
    model_cache_[type] = data;

    return data;
}

// ----------------------------------------------------------------------------------------------------

bool ModelLoader::exists(const std::string& type) const
{
    std::map<std::string, tue::config::DataConstPointer>::const_iterator it = model_cache_.find(type);
    if (it != model_cache_.end())
        return true;

    tue::filesystem::Path model_path = getModelPath(type);
    return model_path.exists();
}

// ----------------------------------------------------------------------------------------------------

bool ModelLoader::create(const UUID& id, const std::string& type, UpdateRequest& req, std::stringstream& error)
{
    tue::config::DataConstPointer data = loadModelData(type, error);
    if (data.empty())
        return false;

    if (!create(data, id, "", req, error))
        return false;

    return true;
}

// ----------------------------------------------------------------------------------------------------

bool ModelLoader::create(const tue::config::DataConstPointer& data, UpdateRequest& req, std::stringstream& error)
{
    if (!create(data, "", "", req, error, ""))
        return false;

    return true;
}

// ----------------------------------------------------------------------------------------------------

bool ModelLoader::create(const tue::config::DataConstPointer& data, const UUID& id_opt, const UUID& parent_id,
                         UpdateRequest& req, std::stringstream& error, const std::string& model_path,
                         const geo::Pose3D& pose_offset)
{
    tue::config::Reader r(data);

    // Get Id
    UUID id;
    std::string id_str;
    if (r.value("id", id_str, tue::config::OPTIONAL))
    {
        if (parent_id.str().empty() || parent_id.str()[0] == '_')
            id = id_str;
        else
            id = parent_id.str() + "/" + id_str;
    }
    else if (!id_opt.str().empty())
    {
        id = id_opt;
    }
    else
    {
        id = ed::Entity::generateID();
    }

    // Get type. If it exists, first construct an entity based on the given type.
    std::string type;
    if (r.value("type", type, tue::config::OPTIONAL))
    {
        tue::config::DataConstPointer super_data = loadModelData(type, error);
        if (super_data.empty())
            return false;

        tue::config::DataPointer data_combined;
        data_combined.add(super_data);
        data_combined.add(data);

        r = tue::config::Reader(data_combined);
    }

    // Set type
    req.setType(id, type);

    geo::Pose3D pose = geo::Pose3D::identity();

    // Get pose
    if (r.readGroup("pose"))
    {
        r.value("x", pose.t.x);
        r.value("y", pose.t.y);
        r.value("z", pose.t.z);

        double rx = 0, ry = 0, rz = 0;
        r.value("X", rx, tue::config::OPTIONAL);
        r.value("Y", ry, tue::config::OPTIONAL);
        r.value("Z", rz, tue::config::OPTIONAL);

        // Set rotation
        pose.R.setRPY(rx, ry, rz);

        r.endGroup();
    }

    pose = pose_offset * pose;

    req.setPose(id, pose);

    // Check the composition
    if (r.readArray("composition"))
    {
        while (r.nextArrayItem())
        {
            if (!create(r.data(), "", id, req, error, "", pose))
                return false;
        }

        r.endArray();
    }

    // Set shape
    if (r.readGroup("shape"))
    {
        std::string shape_model_path = model_path;
        r.value("__model_path__", shape_model_path);

        if (!shape_model_path.empty() && tue::filesystem::Path(shape_model_path).exists())
        {
            geo::ShapePtr shape = loadShape(shape_model_path, r, shape_cache_, error);
            if (shape)
                req.setShape(id, shape);
            else
                return false;
        }

        r.endGroup();
    }

    // Add additional data
    req.addData(id, r.data());

    return true;
}

} // end namespace models

} // end namespace ed

