#pragma once

#include <string>

struct VideoParameters {
    std::string cameraLeftName = "00-00 Pro Capture Dual HDMI (PCI:0005:03:00.0)";
	std::string cameraRightName = "00-01 Pro Capture Dual HDMI (PCI:0005:05:00.0)";
	std::string rtaIpAddr = "127.0.0.1";
    std::string codec = "h264";
    std::string h264Preset = "p2";
    std::string h264Crf = "0";
    int h264GopSize = 5;
    int captureWidth = 1280;
    int captureHeight = 1024;
    int leftOffset = 0;
	int topOffset = 0;
	int rightOffset = 0;
	int bottomOffset = 0;
	int rtaVideoPort = 9601;
	int etaVideoPort = 9601;
	int bitRate = 30;   // Mbps
    int fps = 60;
    int capColorFormat = 0x04;  // yuv2020;
    int enable_crop = 0;
    int cropX = 200;
    int cropY = 0;
    int cropWidth = captureWidth - 2 * cropX;
    int cropHeight = captureHeight * 2;
};
