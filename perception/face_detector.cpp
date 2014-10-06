#include "face_detector.h"

#include "ed/measurement.h"
#include <ed/entity.h>

#include <rgbd/Image.h>
#include <rgbd/View.h>


// ----------------------------------------------------------------------------------------------------

FaceDetector::FaceDetector() :
    PerceptionModule("face_detector"),
    init_success_(false)
{

}


// ----------------------------------------------------------------------------------------------------

FaceDetector::~FaceDetector()
{
    // destroy the debug window
    if (kDebugMode) {
        cv::destroyWindow("Face Detector Output");
    }
}

// ----------------------------------------------------------------------------------------------------

void FaceDetector::loadModel(const std::string& model_name, const std::string& model_path)
{
    /*
    Load any model specific data here
        model_name: the name of the model (e.g. 'human')
        model_path: the directory in which the models are stored
    */
    if (model_name.compare("face") == 0){
        kModuleName = "face_detector";
        kModelName = model_name;
        kCascadePath = model_path + "/cascade_classifiers/";
        kDebugMode = false;

        kClassFrontScaleFactor = 1.2;
        kClassFrontMinNeighbors = 2;
        kClassFrontMinSize = cv::Size(20,20);

        kClassProfileScaleFactor= 1.1;
        kClassProfileMinNeighbors = 2;
        kClassProfileMinSize = cv::Size(20,20);

        // load training files
        if (!classifier_front.load(kCascadePath + "haarcascade_frontalface_default.xml") ||
                !classifier_profile.load(kCascadePath + "haarcascade_profileface.xml")) {

            std::cout << "[" << kModuleName << "] " << "Unable to load all haar cascade files ("<< kCascadePath << ")" << std::endl;
            return;
        }

        if (kDebugMode){
            kDebugFolder = "/tmp/face_detector/";
            CleanDebugFolder(kDebugFolder);

            // create debug window
            cv::namedWindow("Face Detector Output", CV_WINDOW_AUTOSIZE);
        }

        init_success_ = true;
    }
}

// ----------------------------------------------------------------------------------------------------

void FaceDetector::process(ed::EntityConstPtr e, tue::Configuration& result) const
{
    if (!init_success_)
        return;

    // Get the best measurement from the entity
    ed::MeasurementConstPtr msr = e->bestMeasurement();
    if (!msr)
        return;

    std::vector<cv::Rect> faces_front;
    std::vector<cv::Rect> faces_profile;

    // Get the depth and color image from the measurement
    const cv::Mat& depth_image = msr->image()->getDepthImage();
    const cv::Mat& color_image = msr->image()->getRGBImage();

    cv::Mat mask_cv = cv::Mat::zeros(depth_image.rows, depth_image.cols, CV_8UC1);

    // Iterate over all points in the mask
    for(ed::ImageMask::const_iterator it = msr->imageMask().begin(depth_image.cols); it != msr->imageMask().end(); ++it)
    {
        // mask's (x, y) coordinate in the depth image
        const cv::Point2i& p_2d = *it;

        // paint a mask
//        mask_cv.at<unsigned char>(*it) = 255;
        mask_cv.at<unsigned char>(cv::Point2i(ClipInt(p_2d.x + 8, 0, depth_image.cols), p_2d.y)) = 255; // TODO: remove this hack after calibrating the kinect
    }

    // improve the contours of the mask
    OptimizeContour(mask_cv, mask_cv);

    // Detect faces in the measurment and assert the results
    if(DetectFaces(color_image, mask_cv, e->id(), faces_front, faces_profile)){
        result.setValue("type", "face");
        result.setValue("type-score", 1.0);

        // if front faces were detected
        if (faces_front.size() > 0){
            result.writeArray("faces_front");
            for (uint j = 0; j < faces_front.size(); j++) {
                result.nextArrayItem();
                result.setValue("x", faces_front[j].x);
                result.setValue("y", faces_front[j].y);
                result.setValue("width", faces_front[j].width);
                result.setValue("height", faces_front[j].height);
            }
            result.endArray();
        }

        // if profile faces were detected
        if (faces_profile.size() > 0){
            result.writeArray("faces_profile");
            for (uint j = 0; j < faces_profile.size(); j++) {
                result.nextArrayItem();
                result.setValue("x", faces_profile[j].x);
                result.setValue("y", faces_profile[j].y);
                result.setValue("width", faces_profile[j].width);
                result.setValue("height", faces_profile[j].height);
            }
            result.endArray();
        }
    }else{
        // no faces detected
        result.setValue("type", "face");
        result.setValue("type-score", 0.0);
    }
}

// ----------------------------------------------------------------------------------------------------

bool FaceDetector::DetectFaces(const cv::Mat& color_img,
                               const cv::Mat& mask_cv,
                               const std::string& entity_id,
                               std::vector<cv::Rect>& faces_front,
                               std::vector<cv::Rect>& faces_profile) const{

    cv::Mat cascade_img;
    cv::Rect bounding_box;
    std::vector<std::vector<cv::Point> > contour;
    bool face_detected;


    // find the contours of the mask
    findContours(mask_cv, contour, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

    // create a minimum area bounding box
    bounding_box = cv::boundingRect(contour[0]);

    // create a copy of the masked image
    color_img.copyTo(cascade_img, mask_cv);

    // select only the area of the bounding box
    cascade_img(bounding_box).copyTo(cascade_img);

    // increase contrast of the image
    normalize(cascade_img, cascade_img, 0, 255, cv::NORM_MINMAX, CV_8UC1);

    // detect frontal faces
    classifier_front.detectMultiScale(cascade_img, faces_front, kClassFrontScaleFactor, kClassFrontMinNeighbors, 0|CV_HAAR_SCALE_IMAGE, kClassFrontMinSize);

    // only search profile faces if the frontal face detection failed
    if (faces_front.size() == 0){
        classifier_profile.detectMultiScale(cascade_img, faces_profile, kClassProfileScaleFactor, kClassProfileMinNeighbors, 0|CV_HAAR_SCALE_IMAGE, kClassProfileMinSize);
    }

    face_detected = (faces_front.size() > 0 || faces_profile.size() > 0);

    // update the coordinates of the faces found, to the overral image
    for (uint j = 0; j < faces_front.size(); j++) {
        faces_front[j].x += bounding_box.x;
        faces_front[j].y += bounding_box.y;
    }

    for (uint j = 0; j < faces_profile.size(); j++) {
        faces_profile[j].x += bounding_box.x;
        faces_profile[j].y += bounding_box.y;
    }

    // if debug mode is active and faces were found
    if (kDebugMode){
        cv::Mat debugImg;

        color_img.copyTo(debugImg);

        for (uint j = 0; j < faces_front.size(); j++)
            cv::rectangle(debugImg, faces_front[j], cv::Scalar(0, 255, 0), 2, CV_AA);

        for (uint j = 0; j < faces_profile.size(); j++)
            cv::rectangle(debugImg, faces_profile[j], cv::Scalar(0, 0, 255), 2, CV_AA);

//      cv::imwrite(kDebugFolder + entity_id + "_faces.png", debugImg);
        cv::imshow("Face Detector Output", debugImg);
    }

    return face_detected;
}


// ----------------------------------------------------------------------------------------------------

void FaceDetector::OptimizeContour(const cv::Mat& mask_orig, const cv::Mat& mask_opt) const{

    mask_orig.copyTo(mask_opt);

    // blur the contour, also expands it a bit
    for (uint i = 6; i < 18; i = i + 2){
        blur(mask_opt, mask_opt, cv::Size( i, i ), cv::Point(-1,-1) );
    }

    cv::threshold(mask_opt, mask_opt, 50, 255, CV_THRESH_BINARY);
}

// ----------------------------------------------------------------------------------------------------

void FaceDetector::CleanDebugFolder(const std::string& folder){
    if (system(std::string("mkdir " + folder).c_str()) != 0){
        //printf("\nUnable to create output folder. Already created?\n");
    }
    if (system(std::string("rm " + folder + "*.png").c_str()) != 0){
        //printf("\nUnable to clean output folder \n");
    }
}

int FaceDetector::ClipInt(int val, int min, int max) const{
    return val <= min ? min : val >= max ? max : val;
}

ED_REGISTER_PERCEPTION_MODULE(FaceDetector)
