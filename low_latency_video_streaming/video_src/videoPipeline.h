#pragma once

#include <thread>

#include"videoCapture.h"
//#include "videoStats.h"
//#include "videoCoder.h"
#include "syncQueue.h"
#include "videoParameters.h"

// #define LOCAL_RENDERING

class VideoPipeline {

	bool renderRunning = false;
	bool coderRunning = false;
	bool captureRunning = false;
	bool srcRunning = false;

	//std::shared_ptr<VideoCoder> vCoder;
	std::shared_ptr<std::thread> srcThd = nullptr;
	std::shared_ptr<std::thread> capThdLeft = nullptr;
	std::shared_ptr<std::thread> capThdRight = nullptr;
	std::shared_ptr<SyncQueue<unsigned char*>> bufferQueue = nullptr;
	std::shared_ptr<SyncQueue<unsigned char*>> capQueueLeft = nullptr;
	std::shared_ptr<SyncQueue<unsigned char*>> capQueueRight = nullptr;
	//std::shared_ptr<VideoStats> videoStats = nullptr;
	//std::shared_ptr<LatencyStats> latencyStats = nullptr;
	//std::shared_ptr<VideoStats> renderStats = nullptr;

	bool isStereoscopic = true;		// default is stereoscopic
	std::unordered_map<std::string, std::string> camNameMap;
	std::unordered_map<std::string, int> camIdxMap;
	std::unordered_map<std::string, std::shared_ptr<VideoCapture>> camMap;

	unsigned int width;
	unsigned int height;
	double scalingFactor = 1.0;
	unsigned int colorDepth = 888; // BGR 888
	void runSrc();
	void runCaptureLeft();
	void runCaptureRight();

	VideoParameters params;

public:

	void *serverData;

	VideoPipeline();

	bool setCamera(
		const char* camera1,
		const char* camera2,
		const int& cameraIndex1,
		const int& cameraIndex2,
		const int& setWidth,
		const int& setHeight,
		const int& roiWidth,
		const int& roiHeight,
		const int& capColorFormat,
		const int& fps
	);


	void start();
	void stop();
	void reload();
	void render();

	// cv::Mat getBuffer();
	unsigned char* getBuffer();

	int getBufferCapacity();

	bool isSrcRunning() {
		return srcRunning;
	}

	bool isRenderRunning() {
		return renderRunning;
	}

	bool isCoderRunning() {
		return coderRunning;
	}

	unsigned int getVideoWidth() {
		return width;
	}

	unsigned int getVideoHeight() {
		return height;
	}

#if 0
	double getVideoFPS();
	double getVideoBandwidth();

	double getRenderFPS();
	double getRenderBandwidth();

	std::string getVideoCodec() const;
	bool setVideoCodec(const std::string& codec_name);

	std::string getH264Preset() const;
	bool setH264Preset(const std::string& preVal);

	std::string getH264Crf() const;
	bool setH264Crf(const std::string& crfVal);

	int getH264Gop() const;
	bool setH264Gop(const int& gopVal);

	double getVideoBitrate() const;
	bool setVideoBitrate(const double& video_bitrate);

	void showPerformanceStats();
#endif
};