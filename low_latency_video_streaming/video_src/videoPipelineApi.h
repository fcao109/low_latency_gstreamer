#pragma once

#include <opencv2/opencv.hpp>

void start();
void stop();
void reload();
void render();
unsigned char* getBuffer();

#if 0
void getStats();
std::string getVideoCodec();
bool setVideoCodec(const std::string codec_name);
bool setH264Preset(const std::string presetVal);
bool setH264Crf(const std::string crfVal);
bool setH264Gop(const int gopVal);
std::string getH264Preset();
std::string getH264Crf();
int getH264Gop();
double getVideoBitrate();
bool setVideoBitrate(const double video_bitrate);
double getVideoFps();
double getVideoBandwidth();
#endif

bool OpenLiveCaptureSources(int setWidth,
							int setHeight,
							const char* camera1,
							const char* camera2,
							int paddingLeft,
							int paddingTop,
							int paddingRight,
							int paddingBottom,
							int colorFormat,
							int fps);

int runVideoPipeline(void *data);

void yuan_main();