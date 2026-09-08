#include <string>
#include <vector>
#include <regex>
#include <iostream>
#include <array>
#include <cstdio>

#include <opencv2/core/core.hpp>

#include "utils.h"
#include "videoPipeline.h"
#include "videoPipelineApi.h"

VideoPipeline vSrc = VideoPipeline();
std::shared_ptr<std::thread> runThd = nullptr;
std::shared_ptr<std::thread> renderThd = nullptr;

extern void *yuan_serverData;

void runStart() {

	if (vSrc.isSrcRunning()) {
		return;
	}

	vSrc.start();

}
void stop() {

	if (!vSrc.isSrcRunning()) {
		return;
	}

	vSrc.stop();

}

void reload() {
	vSrc.reload();

}

void render() {
    vSrc.render();
}

unsigned char* getBuffer() {
    return vSrc.getBuffer();
}

#if 0
void getStats() {
    vSrc.showPerformanceStats();
}

std::string getVideoCodec() {
    return vSrc.getVideoCodec();
}
bool setVideoCodec(const std::string codec_name) {
    return vSrc.setVideoCodec(codec_name);
}
double getVideoBitrate() {
    return vSrc.getVideoBitrate();
}
bool setVideoBitrate(const double video_bitrate) {
    return vSrc.setVideoBitrate(video_bitrate);
}

bool setH264Preset(const std::string presetVal) {
    return vSrc.setH264Preset(presetVal);
}

bool setH264Crf(const std::string crfVal) {
    return vSrc.setH264Crf(crfVal);
}

bool setH264Gop(const int gopVal) {
    if (gopVal < 0 || gopVal > 200) {
        return false;
    }
    return vSrc.setH264Gop(gopVal);
}

int getH264Gop() {
    return vSrc.getH264Gop();
}

std::string getH264Preset() {
    return vSrc.getH264Preset();
}

std::string getH264Crf() {
    return vSrc.getH264Crf();
}

double getVideoFps() {
    return vSrc.getVideoFPS();
}

double getVideoBandwidth() {
    return vSrc.getVideoBandwidth();
}
#endif

int extract_device_index(const std::string& device_path) {
    std::regex pattern("/dev/video(\\d+)");
    std::smatch matches;
    if (std::regex_search(device_path, matches, pattern) && matches.size() > 1) {
        return std::stoi(matches[1].str());
    }
    return -1; // Return -1 if no match found
}

// Command line execution
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

static int GetCameraIndexByName(const std::string& cameraName) {
    std::string command = "v4l2-ctl --list-devices";
    std::string output = exec(command.c_str());
    std::istringstream iss(output);
    std::string line;
    bool next_line_is_device = false;

     while (std::getline(iss, line)) {
        if (next_line_is_device) {
            return extract_device_index(line);
        }
        if (line.find(cameraName) != std::string::npos) {
            next_line_is_device = true;
        }
    }

    return -1;
}

bool OpenLiveCaptureSources(int setWidth, int setHeight, const char* camera1, const char* camera2, int paddingLeft, int paddingTop, int paddingRight, int paddingBottom, int capColorFormat, int fps)
{
    int roiWidth = setWidth - (paddingLeft + paddingRight);
    int roiHeight = setHeight - (paddingTop + paddingBottom);
    int x_Offset = paddingLeft;
    int y_Offset = paddingTop;

    int cameraIndex1 = -1, cameraIndex2 = -1;
    cameraIndex1 = GetCameraIndexByName(std::string(camera1));
    cameraIndex2 = GetCameraIndexByName(std::string(camera2));

    if (cameraIndex1 == -1 || cameraIndex2 == -1)
        return false;

    std::cout << "cameraIndex1=" << cameraIndex1 << std::endl;
    std::cout << "cameraIndex2=" << cameraIndex2 << std::endl;

    if (!vSrc.setCamera(camera1, camera2, cameraIndex1, cameraIndex2, setWidth, setHeight, roiWidth, roiHeight, capColorFormat, fps))
        return false;

    return true;
}

#if 0
int runVideoPipeline(void *data) {
    VideoParameters params = VideoParameters();
    if (!(OpenLiveCaptureSources(params.captureWidth,
                                params.captureHeight,
                                params.cameraLeftName.c_str(),
                                params.cameraRightName.c_str(),
                                params.leftOffset,
                                params.topOffset,
                                params.rightOffset,
                                params.bottomOffset,
                                params.capColorFormat,
                                params.fps))) {

        logMessage("Error: Can't open camera, fail to start:(");
        return 1;
    }

    vSrc.serverData = data;

    runThd = std::make_shared<std::thread>(&runStart);
    // usleep(1000000);
#ifdef LOCAL_RENDERING
    renderThd = std::make_shared<std::thread>(&render);
#endif

    return 0;
}
#else
int runVideoPipeline(void *data) {
    VideoParameters params = VideoParameters();

    yuan_serverData = data;

    runThd = std::make_shared<std::thread>(&yuan_main);
    usleep(1000000);

    return 0;
}
#endif