#include "shape_loader.h"

#include <tue/filesystem/path.h>
#include <ros/package.h>
#include <fstream>

#include <geolib/serialization.h>
#include <geolib/HeightMap.h>
#include <geolib/Shape.h>

#include <yaml-cpp/yaml.h>

#include <opencv2/highgui/highgui.hpp>

#include <geolib/Importer.h>

#include "xml_shape_parser.h"

namespace ed
{
namespace models
{

// ----------------------------------------------------------------------------------------------------

geo::ShapePtr getHeightMapShape(const tue::filesystem::Path& path, tue::config::Reader r)
{
    geo::ShapePtr shape;

    double resolution, origin_x, origin_y, origin_z, blockheight;
    if (r.value("origin_x", origin_x) &&
            r.value("origin_y", origin_y) &&
            r.value("origin_z", origin_z) &&
            r.value("resolution", resolution) &&
            r.value("blockheight", blockheight))
    {
        cv::Mat image = cv::imread(path.string(), CV_LOAD_IMAGE_GRAYSCALE);   // Read the file

        std::vector<std::vector<double> > map;

        if (image.data)
        {
            map.resize(image.cols);
            for(int x = 0; x < image.cols; ++x) {
                map[x].resize(image.rows);
                for(int y = 0; y < image.rows; ++y) {
                    map[x][image.rows - y - 1] = blockheight - (double)image.at<unsigned char>(y, x) / 255 * blockheight;
                }
            }
            geo::HeightMap hmap = geo::HeightMap::fromGrid(map, resolution);
            geo::Mesh mesh = hmap.getMesh().getTransformed(geo::Transform(origin_x, origin_y, origin_z));
            shape = geo::ShapePtr(new geo::Shape());
            shape->setMesh(mesh);
        }
        else
        {
            std::cout << "ed::models::getHeightMapShape() : ERROR loading heightmap at '" << path.string() << "'. Image constains invalid data." << std::endl;
        }
    }
    else
    {
        std::cout << "ed::models::getHeightMapShape() : ERROR while loading heightmap paramaters at '" << path.string() << "'. Required shape parameters: resolution, origin_x, origin_y, origin_z, blockheight" << std::endl;
    }

    return shape;
}

// ----------------------------------------------------------------------------------------------------

geo::ShapePtr loadShape(const std::string& model_path, tue::config::Reader cfg, std::map<std::string, geo::ShapePtr>& shape_cache)
{
    geo::ShapePtr shape;

    std::string path;
    if (cfg.value("path", path))
    {
        tue::filesystem::Path shape_path(model_path + "/" + path);

        // Check cache first
        std::map<std::string, geo::ShapePtr>::const_iterator it = shape_cache.find(shape_path.string());
        if (it != shape_cache.end())
        {
            std::cout << "Use shape cache: " << shape_path.string() << std::endl;
            return it->second;
        }

        if (shape_path.exists())
        {
            std::string xt = shape_path.extension();
            if (xt == ".pgm")
            {
                shape = getHeightMapShape(shape_path, cfg);
            }
            else if (xt == ".geo")
            {
                geo::serialization::registerDeserializer<geo::Shape>();
                shape = geo::serialization::fromFile(shape_path.string());
            }
            else if (xt == ".3ds")
            {
                shape = geo::Importer::readMeshFile(shape_path.string());
            }
            else if (xt == ".xml")
            {
                std::string error;
                shape = parseXMLShape(shape_path.string(), error);
            }

            if (!shape)
                std::cout << "ed::models::loadShape() : ERROR while loading shape at " << shape_path.string() << std::endl;
            else
                // Add to cache
                shape_cache[shape_path.string()] = shape;
        }
        else
        {
            std::cout << "ed::models::loadShape() : ERROR while loading shape of at " << shape_path.string() << " ; file does not exist" << std::endl;
        }
    }
    else
    {
        std::cout << "ed::models::loadShape() : ERROR while loading shape, no path specified in model.yaml" << std::endl;
    }

    return shape;
}

}
}
