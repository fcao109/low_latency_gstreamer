#pragma once

#include<semaphore.h>
#include<cstdint>
#include<iostream>
#include<string>
#include<cstring>

#include "MWFOURCC.h"
#include "LibMWCapture/MWCapture.h"

#if defined(USE_JETSON_ZEROCOPY) || defined(USE_GPUDIRECT_RDMA)
#include <cuda_runtime.h>
#endif

#ifdef USE_GPUDIRECT_RDMA
#include "LibMWCapture/MWDMAMem.h"
#endif


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

    MWCAP_VIDEO_BUFFER_INFO VideoBufferInfo;
    MWCAP_VIDEO_FRAME_INFO VideoFrameInfo;
    MWCAP_VIDEO_CAPTURE_STATUS captureStatus;

    unsigned char *pucFrmBuf = nullptr;
    static int thread_count;

#ifdef USE_GPUDIRECT_RDMA
    void *gpuDevPtr = nullptr;          // cudaMalloc'd device pointer
    LARGE_INTEGER gpuPhysAddr;          // physical address for RDMA
    bool gpuDirectReady = false;
#endif

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

    // Original API: captures into the caller's buffer (copies from internal buf).
    void read(unsigned char* frameBuf);

    // Zero-copy API: captures into the internal buffer and returns a pointer to
    // it directly.  The pointer remains valid until the next readDirect() call
    // on the same VideoCapture instance.  On the Jetson path the buffer is
    // cudaMallocHost'd, so the GPU can read it without a PCIe upload.
    unsigned char* readDirect();

    // Size of a single captured frame in bytes.
    DWORD getImageSize() const { return dwImageSize; }

#ifdef USE_GPUDIRECT_RDMA
    // GPUDirect: returns the device pointer that the Magewell card DMA'd into.
    void* getGpuDevPtr() const { return gpuDevPtr; }
    bool isGpuDirectReady() const { return gpuDirectReady; }
#endif
};