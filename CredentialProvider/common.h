//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// This file contains some global variables that describe what our
// sample tile looks like.  For example, it defines what fields a tile has
// and which fields show in which states of LogonUI. This sample illustrates
// the use of each UI field type.

#pragma once
#include "helpers.h"
#include <iostream>
#include <string>
#include <vector>
#include <stdlib.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/face.hpp>
#include "LuxandFaceSDK.h"
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

const std::string caffeConfigFile = "c:/Program Files (x86)/FaceCredential/model/deploy.prototxt";
const std::string caffeWeightFile = "c:/Program Files (x86)/FaceCredential/model/res10_300x300_ssd_iter_140000_fp16.caffemodel";

const std::string tensorflowConfigFile = "c:/Program Files (x86)/FaceCredential/model/opencv_face_detector.pbtxt";
const std::string tensorflowWeightFile = "c:/Program Files (x86)/FaceCredential/model/opencv_face_detector_uint8.pb";
const std::string trainModelFile = "c:/Program Files (x86)/FaceCredential/model/train-model.xml";

// The indexes of each of the fields in our credential provider's tiles. Note that we're
// using each of the nine available field types here.
enum SAMPLE_FIELD_ID
{
    SFI_TILEIMAGE         = 0,
    SFI_LABEL             = 1,
    SFI_TRYAGAIN_CMD  = 2,
	SFI_PASSWORD          = 3,
    SFI_SUBMIT_BUTTON     = 4,
	SFI_NUM_FIELDS        = 5,  // Note: if new fields are added, keep NUM_FIELDS last.  This is used as a count of the number of fields
};

// The first value indicates when the tile is displayed (selected, not selected)
// the second indicates things like whether the field is enabled, whether it has key focus, etc.
struct FIELD_STATE_PAIR
{
    CREDENTIAL_PROVIDER_FIELD_STATE cpfs;
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE cpfis;
};

// These two arrays are seperate because a credential provider might
// want to set up a credential with various combinations of field state pairs
// and field descriptors.

// The field state value indicates whether the field is displayed
// in the selected tile, the deselected tile, or both.
// The Field interactive state indicates when
static const FIELD_STATE_PAIR s_rgFieldStatePairs[] =
{
    { CPFS_DISPLAY_IN_BOTH,            CPFIS_NONE    },    // SFI_TILEIMAGE
    { CPFS_HIDDEN,                     CPFIS_NONE    },    // SFI_LABEL
    { CPFS_DISPLAY_IN_BOTH,   CPFIS_NONE    },    // SFI_TRYAGAIN_CMD
	{ CPFS_DISPLAY_IN_SELECTED_TILE,   CPFIS_NONE },    // SFI_PASSWORD
    { CPFS_DISPLAY_IN_SELECTED_TILE,   CPFIS_NONE    },    // SFI_SUBMIT_BUTTON  
	
	
};

// Field descriptors for unlock and logon.
// The first field is the index of the field.
// The second is the type of the field.
// The third is the name of the field, NOT the value which will appear in the field.
static const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR s_rgCredProvFieldDescriptors[] =
{
    { SFI_TILEIMAGE,         CPFT_TILE_IMAGE,    L"Image",                   },
    { SFI_LABEL,             CPFT_SMALL_TEXT,    L"Tooltip",                  },
	{ SFI_TRYAGAIN_CMD,      CPFT_COMMAND_LINK,    L"Logon status: "                                             },
    { SFI_PASSWORD,          CPFT_PASSWORD_TEXT, L"recovery password"                                              },
    { SFI_SUBMIT_BUTTON,     CPFT_SUBMIT_BUTTON, L"Submit"                                                     },
    
};


static const PWSTR s_RegistryKey_face = L"SOFTWARE\\FaceLogin\\FaceCredentialProvider";
static const PWSTR s_WinlogonKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon";
static const PWSTR s_WinLogonValue = L"Shell";
static const PWSTR s_WinLogonValueDefault = L"explorer.exe";
static const PWSTR s_WinLogonValueChanged = L"ConfigurableShell.exe";
static const PWSTR description_string = L"fivestarsmobi";
static const PWSTR failMessages[4] = {
    L"No have Camera.Click after connect camera",
    L"Face login faild.Try again",
    L"Face login faild again.Try again",
    L"Face login faild three time.\n Try login with recovery password"
};