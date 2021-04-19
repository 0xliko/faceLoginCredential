// pch.cpp: source file corresponding to the pre-compiled header

#include "pch.h"
HBITMAP ConvertCVMatToBMP(cv::Mat frame)
{
	Mat dest(frame.cols, frame.rows, CV_8UC3);
	frame.copyTo(dest(Rect(0, 0, frame.cols, frame.rows)));
	frame = dest;
	resize(frame, frame, Size(frame.cols / 4 * 4, frame.rows / 4 * 4));
	auto convertOpenCVBitDepthToBits = [](const int32_t value)
	{
		auto regular = 0u;

		switch (value)
		{
		case CV_8U:
		case CV_8S:
			regular = 8u;
			break;

		case CV_16U:
		case CV_16S:
			regular = 16u;
			break;

		case CV_32S:
		case CV_32F:
			regular = 32u;
			break;

		case CV_64F:
			regular = 64u;
			break;

		default:
			regular = 0u;
			break;
		}

		return regular;
	};

	auto imageSize = frame.size();
	assert(imageSize.width && "invalid size provided by frame");
	assert(imageSize.height && "invalid size provided by frame");

	if (imageSize.width && imageSize.height)
	{
		auto headerInfo = BITMAPINFOHEADER{};
		ZeroMemory(&headerInfo, sizeof(headerInfo));

		headerInfo.biSize = sizeof(headerInfo);
		headerInfo.biWidth = imageSize.width;
		headerInfo.biHeight = -(imageSize.height); // negative otherwise it will be upsidedown
		headerInfo.biPlanes = 1;// must be set to 1 as per documentation frame.channels();

		const auto bits = convertOpenCVBitDepthToBits(frame.depth());
		headerInfo.biBitCount = frame.channels() * bits;

		auto bitmapInfo = BITMAPINFO{};
		ZeroMemory(&bitmapInfo, sizeof(bitmapInfo));

		bitmapInfo.bmiHeader = headerInfo;
		bitmapInfo.bmiColors->rgbBlue = 0;
		bitmapInfo.bmiColors->rgbGreen = 0;
		bitmapInfo.bmiColors->rgbRed = 0;
		bitmapInfo.bmiColors->rgbReserved = 0;

		auto dc = GetDC(nullptr);
		assert(dc != nullptr && "Failure to get DC");
		auto bmp = CreateDIBitmap(dc,
			&headerInfo,
			CBM_INIT,
			frame.data,
			&bitmapInfo,
			DIB_RGB_COLORS);
		assert(bmp != nullptr && "Failure creating bitmap from captured frame");

		return bmp;
	}
	else
	{
		return nullptr;
	}
}
int detectFaceOpenCVDNN(Net net, Mat& frameOpenCVDNN,Rect& faceRect)
{
	int frameHeight = frameOpenCVDNN.rows;
	int frameWidth = frameOpenCVDNN.cols;
#ifdef CAFFE
	cv::Mat inputBlob = cv::dnn::blobFromImage(frameOpenCVDNN, inScaleFactor, cv::Size(inWidth, inHeight), meanVal, false, false);
#else
	cv::Mat inputBlob = cv::dnn::blobFromImage(frameOpenCVDNN, inScaleFactor, cv::Size(inWidth, inHeight), meanVal, true, false);
#endif

	net.setInput(inputBlob, "data");
	cv::Mat detection = net.forward("detection_out");

	cv::Mat detectionMat(detection.size[2], detection.size[3], CV_32F, detection.ptr<float>());
	int count = 0;
	for (int i = 0; i < detectionMat.rows; i++)
	{
		float confidence = detectionMat.at<float>(i, 2);

		if (confidence > confidenceThreshold)
		{

			int x1 = static_cast<int>(detectionMat.at<float>(i, 3) * frameWidth);
			int y1 = static_cast<int>(detectionMat.at<float>(i, 4) * frameHeight);
			int x2 = static_cast<int>(detectionMat.at<float>(i, 5) * frameWidth);
			int y2 = static_cast<int>(detectionMat.at<float>(i, 6) * frameHeight);
			if (detectionMat.at<float>(i, 5) >= 1 || detectionMat.at<float>(i, 6) >= 1
				|| detectionMat.at<float>(i, 3) >= 1 || detectionMat.at<float>(i, 4) >= 1)
				continue;
			if (detectionMat.at<float>(i, 5) <= 0 || detectionMat.at<float>(i, 6) <= 0
				|| detectionMat.at<float>(i, 3) <= 0 || detectionMat.at<float>(i, 4) <= 0)
				continue;
			const int top = min(y1, y2);
			const int left = min(x1, x2);
			const int width = abs(x1 - x2);
			const int height = abs(y1 - y2);
			if (width == 0 || height == 0) continue;
			if (width > frameOpenCVDNN.rows  * 178 / 218.0) continue;
			int patternWidth = frameOpenCVDNN.cols * 178 / 218.0;
			int patternLeft = left + width / 2 - patternWidth/2;
			if (patternLeft < 0) patternLeft = 0;
			if (patternLeft + patternWidth > frameOpenCVDNN.cols) patternLeft = frameOpenCVDNN.cols - patternWidth;
			faceRect = Rect(left,top,width,height);// patternLeft, 0, patternWidth, frameOpenCVDNN.rows);
            count++;
			cv::rectangle(frameOpenCVDNN, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(255, 0, 0), 2, 4);
		}
	}
	return count;

}

// When you are using pre-compiled headers, this source file is necessary for compilation to succeed.
