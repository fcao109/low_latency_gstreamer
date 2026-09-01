#pragma once

#include<semaphore.h>
#include<cstdint>
#include<iostream>
#include<string>
#include<cstring>

#include "MWFOURCC.h"
#include "LibMWCapture/MWCapture.h"


class VideoCapture {
    std::string camName;
    int camIndex;

    int capWidth = 1280;
    int capHeight = 1024;
    const uint32_t uiFourCC = 0x30323449;   // use I420, YUV:420
    MWCAP_VIDEO_COLOR_FORMAT capColorFormat = MWCAP_VIDEO_COLOR_FORMAT_YUV2020;     // default

    HCHANNEL hChannel;
    MWCAP_PTR hCaptureEvent = 0;
    MWCAP_PTR hNotifyEvent = 0;
    HNOTIFY hNotify = 0;
    ULONGLONG ullStatusBits = 0;
    uint32_t uiStartCapture = 0;

    DWORD dwMinStride;
    DWORD dwImageSize;

    // VideoRenderXCB *render = nullptr;
    //= new VideoRenderXCB;

    MWCAP_VIDEO_BUFFER_INFO VideoBufferInfo;
    MWCAP_VIDEO_FRAME_INFO VideoFrameInfo;
    MWCAP_VIDEO_CAPTURE_STATUS captureStatus;

    unsigned char *pucFrmBuf = nullptr;
    // static sem_t g_Sem;
    static int thread_count;

    HCHANNEL OpenChannel();

public:
    VideoCapture(int cam_index=0, int cap_width=1280, int cap_height=1024, int cap_color_format=0x04) {
        camIndex = cam_index;
        capWidth = cap_width;
        capHeight = cap_height;
        capColorFormat = (MWCAP_VIDEO_COLOR_FORMAT) cap_color_format;
        if (capColorFormat == MWCAP_VIDEO_COLOR_FORMAT_RGB) {
            dwMinStride=FOURCC_CalcMinStride(MWFOURCC_BGR24, capWidth, 4);
            dwImageSize=FOURCC_CalcImageSize(MWFOURCC_BGR24, capWidth, capHeight, dwMinStride);
        } else {
            dwMinStride = FOURCC_CalcMinStride(uiFourCC, capWidth, 4);
            dwImageSize = FOURCC_CalcImageSize(uiFourCC, capWidth, capHeight, dwMinStride);
        }
    }

    ~VideoCapture();

    int open();
    bool isOpened();
    void release();
    void read(unsigned char* frameBuf);

};