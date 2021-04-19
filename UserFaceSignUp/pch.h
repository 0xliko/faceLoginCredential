// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here
#include "framework.h"
#include <iostream>
#include <string>
#include <vector>
#include <stdlib.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/face.hpp>

using namespace cv;
using namespace std;
using namespace cv::dnn;
using namespace cv::face;

const size_t inWidth = 320;
const size_t inHeight = 320;
const double inScaleFactor = 1.0;
const float confidenceThreshold = 0.7;
const cv::Scalar meanVal(104.0, 177.0, 123.0);

#define CAFFE

const std::string caffeConfigFile = "./model/deploy.prototxt";
const std::string caffeWeightFile = "./model/res10_300x300_ssd_iter_140000_fp16.caffemodel";

const std::string tensorflowConfigFile = "./model/opencv_face_detector.pbtxt";
const std::string tensorflowWeightFile = "./model/opencv_face_detector_uint8.pb";

static const PWSTR s_RegistryKey_face = L"SOFTWARE\\FaceLogin\\FaceCredentialProvider";
static const LPWSTR description_string = L"fivestarsmobi";

HBITMAP ConvertCVMatToBMP(cv::Mat frame);
int detectFaceOpenCVDNN(Net net, Mat& frameOpenCVDNN,Rect& facerect);
#endif //PCH_H
