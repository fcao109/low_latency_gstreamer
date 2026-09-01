#pragma once

#include <iostream>
#include <chrono>
#include <iomanip>
#include <thread>
#include <string>
#include <vector>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <opencv2/opencv.hpp>

#if !(defined(_WIN32) || defined(WIN32))
#include <ctime>
#endif

#define horizontal3D 0
#define profiling 1

//#define CPU_ONLY_TEST

inline void logMessage(const std::string& message) {
#if defined(_WIN32) || defined(WIN32)
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto value = now_ms.time_since_epoch().count();
    auto ms = value % 1000;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now_ms.time_since_epoch());
    auto time = std::chrono::system_clock::to_time_t(std::chrono::time_point<std::chrono::system_clock>(seconds));

    std::tm timeinfo;

    localtime_s(&timeinfo, &time);

    std::cout << std::put_time(&timeinfo, "[%m/%d %H:%M:%S:") << std::setfill('0') << std::setw(3) << ms << "] " << message << std::endl;
#else
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms =  std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())%1000;
    //format time
    std::tm now_tm = *std::localtime(&now_time_t);
    //std::string thd_msg = "tID:" + std::to_string(std::this_thread::get_id()) + " " + message;
    std::cout << std::put_time(&now_tm, "[%m/%d %H:%M:%S:") << std::setfill('0') << std::setw(3) << now_ms.count() << "] " << message << std::endl;
#endif
}

long long getCurrentUTCEpochMillis();
bool rectifyRoi(cv::Rect& roi, cv::Mat& img);
void sleepForSeconds(double seconds);
void overlayTimestamp(cv::Mat &frame, std::string& ts, cv::Point& pos);
void stitchCVFrames(cv::Mat& image1, cv::Mat& image2, cv::Mat& canvas);
void cropFrames(unsigned char* srcBuffer,
                int srcWidth, int srcHeight,
                int cropX, int cropY, int cropWidth, int cropHeight,
                unsigned char* destBuffer);
void stitchFrames(unsigned char* frameBufLeft, unsigned char* frameBufRight, int width, int height, unsigned char* outputBuf, int capColorFormat);
bool isColordepthCorrect(const int& colorDepth);
cv::Mat reduceBitDepth(const cv::Mat& img, int bitDepth);
unsigned char** allocate_images(int width, int height, int num_images, int capColorFormat);
cv::Mat convertYUV420Frame(unsigned char* yuvData, int width, int height);
void resizeYuv420(unsigned char* srcYuv, int srcWidth, int srcHeight, unsigned char* dstYuv, int dstWidth, int dstHeight);
