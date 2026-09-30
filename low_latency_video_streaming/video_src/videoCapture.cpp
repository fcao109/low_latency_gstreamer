#include "videoCapture.h"

#define NUM_PARTIAL_LINE 0

// sem_t VideoCapture::g_Sem;  // Definition of the static variable
int VideoCapture::thread_count = 0;
VideoCapture::~VideoCapture() {
    std::cout << "VideoCapture " << camIndex << " is destroyed.\n";
}


HCHANNEL VideoCapture::OpenChannel() {
    HCHANNEL hChannel = NULL;
    int nChannelCount = MWGetChannelCount();

    if (0 == nChannelCount) {
        std::cerr <<"ERROR: Can't find channels!\n";
        return nullptr;
    }

    std::cout << "Find " << nChannelCount <<" channels!\n";

    int nProDevCount = 0;
    int nProDevChannel[32] = {-1};
    for (int i = 0; i < nChannelCount; i++){
        MWCAP_CHANNEL_INFO info;
        MW_RESULT mr = MWGetChannelInfoByIndex(i, &info);
        if (0 == strcmp(info.szFamilyName, "Pro Capture")){
            nProDevChannel[nProDevCount] = i;
            nProDevCount++;
        }
    }
    if (nProDevCount <= 0){
        std::cerr << "\nERROR: Can't find pro channels!\n";
        return nullptr;
    }

    if(camIndex >= nProDevCount){
        std::cout << "ERROR: just have " << nProDevCount << " channel!\n";
        return nullptr;
    }

    std::cout << "Find " << nProDevCount <<" pro channels.\n";

    MWCAP_CHANNEL_INFO videoInfo = { 0 };
    char path[128] = {0};
    MWGetDevicePath(nProDevChannel[camIndex], path);
    hChannel = MWOpenChannelByPath(path);
    std::cout << "OpenChannel by index: " << camIndex << "\n";
    if (hChannel == nullptr) {
        std::cerr << "ERROR: Open channel " << camIndex <<  "error!\n";
        return nullptr;
    }

    if (MW_SUCCEEDED != MWGetChannelInfo(hChannel, &videoInfo)) {
        std::cerr << "ERROR: Can't get channel info!\n";
        return nullptr;
    }

    printf("Open channel - BoardIndex = %X, ChannelIndex = %d.\n", videoInfo.byBoardIndex, videoInfo.byChannelIndex);
    printf("Product Name: %s\n", videoInfo.szProductName);
    printf("Board SerialNo: %s\n\n", videoInfo.szBoardSerialNo);
    return hChannel;
}


int VideoCapture::open() {

    if (VideoCapture::thread_count == 0) {
        if(!MWCaptureInitInstance()){
            std::cerr << "MWCaptureInitInstance chanbnel=" << camIndex << " fails:(\n";
            return -1;
        }
        std::cout << "MWCaptureInitInstance channel=" << camIndex << " OK!\n";
    }
    VideoCapture::thread_count++;

    // if(sem_init(&g_Sem, 0, 0) == -1) {
    //     std::cerr << "Sem initial fails\n";
    //     return -1;
    // }

    MWRefreshDevice();

#if defined(USE_JETSON_ZEROCOPY)
    // Pinned (page-locked) memory: the GPU can read it directly on Jetson's
    // unified memory without a PCIe upload, and the Magewell DMA benefits from
    // the non-pageable target.
    cudaError_t cerr = cudaMallocHost((void**)&pucFrmBuf, dwImageSize);
    if (cerr != cudaSuccess || pucFrmBuf == nullptr) {
        std::cerr << "cudaMallocHost failed: " << cudaGetErrorString(cerr) << "\n";
        return -1;
    }
    std::cout << "Capture buffer: cudaMallocHost " << dwImageSize << " bytes (zero-copy)\n";
#elif defined(USE_GPUDIRECT_RDMA)
    // Allocate GPU device memory and obtain its physical address for RDMA.
    cudaError_t cerr = cudaMalloc(&gpuDevPtr, dwImageSize);
    if (cerr != cudaSuccess || gpuDevPtr == nullptr) {
        std::cerr << "cudaMalloc failed: " << cudaGetErrorString(cerr) << "\n";
        return -1;
    }
    // Get the physical address that the Magewell card will DMA into.
    // This requires the nvidia_p2p kernel API (discrete GPU only).
    cudaPointerAttributes attr;
    cerr = cudaPointerGetAttributes(&attr, gpuDevPtr);
    if (cerr != cudaSuccess) {
        std::cerr << "cudaPointerGetAttributes failed: " << cudaGetErrorString(cerr) << "\n";
        cudaFree(gpuDevPtr);
        gpuDevPtr = nullptr;
        return -1;
    }
    // The physical address is obtained at the kernel level by
    // NvRdmaForProCapture; we pass the device pointer and the module resolves
    // the physical pages.  For the MWCaptureVideoFrameToPhysicalAddressEx API
    // we store the device pointer value as the "physical address" — the
    // NvRdmaForProCapture module intercepts it via the registered
    // MWCAP_VIDEO_MEMORY_TYPE_NVRDMA client.
    gpuPhysAddr.QuadPart = (LONGLONG)(uintptr_t)gpuDevPtr;
    gpuDirectReady = true;
    std::cout << "Capture buffer: cudaMalloc " << dwImageSize
              << " bytes on GPU (GPUDirect RDMA)\n";

    // Also allocate a CPU-side buffer for the legacy read() fallback.
    pucFrmBuf = (unsigned char*)malloc(dwImageSize);
    if (pucFrmBuf == nullptr) {
        std::cerr << "Allocation of fallback pucFrmBuf fails\n";
        return -1;
    }
#else
    pucFrmBuf = (unsigned char*)malloc(dwImageSize);
    if(nullptr == pucFrmBuf){
        std::cerr << "Allocation of pucFrmBuf fails\n";
        return -1;
    }
#endif

    hChannel = OpenChannel();
    if (!hChannel) {
        std::cerr << "OpenChannel fails\n";
        return -1;
    }

    hCaptureEvent = MWCreateEvent();
    if (hCaptureEvent == 0){
        std::cerr << "Error: Create capture event error\n";
        return -1;
    }

    hNotifyEvent = MWCreateEvent();
    if (hNotifyEvent == 0){
        printf("Error: Create capture event error\n");
        return -1;
    }

    if (MWStartVideoCapture(hChannel, hCaptureEvent) != MW_SUCCEEDED) {
        printf("Error: Open Video Capture error!\n");
        return -1;
    }

    std::cout << "VideoCapture::open channel=" << camIndex << " MWStartVideoCapture OK!\n";

    uiStartCapture = 1;

    hNotify = MWRegisterNotify(hChannel, hNotifyEvent, MWCAP_NOTIFY_VIDEO_FRAME_BUFFERING);

#ifdef USE_GPUDIRECT_RDMA
    // GPUDirect: the NvRdmaForProCapture module handles the pinning of GPU
    // memory.  We still pin the CPU fallback buffer for the legacy read() path.
    MWPinVideoBuffer(hChannel, (MWCAP_PTR)pucFrmBuf, dwImageSize);
#else
    MWPinVideoBuffer(hChannel, (MWCAP_PTR)pucFrmBuf, dwImageSize);
#endif
    printf("ImageSize: %d\n", dwImageSize);
    printf("ImageMinStride: %d\n", dwMinStride);
    return 0;

}

void VideoCapture::release() {
     if(hNotify){
        MWUnregisterNotify(hChannel, hNotify);
        hNotify=0;
    }
    if(uiStartCapture){
        MWStopVideoCapture(hChannel);
    }

    if(hNotifyEvent!=0){
        MWCloseEvent(hNotifyEvent);
        hNotifyEvent=0;
    }

    if(hCaptureEvent!=0){
        MWCloseEvent(hCaptureEvent);
        hCaptureEvent=0;
    }

    if (hChannel != NULL){
        MWCloseChannel(hChannel);
        hChannel=NULL;
    }

#if defined(USE_JETSON_ZEROCOPY)
    if (pucFrmBuf) {
        cudaFreeHost(pucFrmBuf);
        pucFrmBuf = nullptr;
    }
#elif defined(USE_GPUDIRECT_RDMA)
    if (gpuDevPtr) {
        cudaFree(gpuDevPtr);
        gpuDevPtr = nullptr;
        gpuDirectReady = false;
    }
    if (pucFrmBuf) {
        free(pucFrmBuf);
        pucFrmBuf = nullptr;
    }
#else
    if (pucFrmBuf) {
        free(pucFrmBuf);
        pucFrmBuf = nullptr;
    }
#endif

    if (VideoCapture::thread_count > 0) {
        MWCaptureExitInstance();
        VideoCapture::thread_count--;
    }

}

bool VideoCapture::isOpened() {
    return uiStartCapture == 1;
}

void VideoCapture::read(unsigned char* frameBuf) {

    // read loop
    while(1) {
        if (MWWaitEvent(hNotifyEvent, 1000) <= 0){
            continue;
        }
        if ((MWGetNotifyStatus(hChannel, hNotify, &ullStatusBits) != MW_SUCCEEDED)){
            continue;
        }
        if (MWGetVideoBufferInfo(hChannel, &VideoBufferInfo) != MW_SUCCEEDED){
            continue;
        }
        if (MWGetVideoFrameInfo(hChannel, VideoBufferInfo.iNewestBuffering, &VideoFrameInfo) != MW_SUCCEEDED){
            continue;
        }

        switch (capColorFormat) {
            case MWCAP_VIDEO_COLOR_FORMAT_YUV2020:
                MWCaptureVideoFrameToVirtualAddressEx(hChannel,
                    VideoBufferInfo.iNewestBuffering,
                    pucFrmBuf, dwImageSize, dwMinStride,
                    0,
                    0,
                    uiFourCC,
                    capWidth,
                    capHeight,
                    0,
                    NUM_PARTIAL_LINE,
                    0,
                    0,
                    0,
                    100,
                    0,
                    100,
                    0,
                    MWCAP_VIDEO_DEINTERLACE_BLEND,
                    MWCAP_VIDEO_ASPECT_RATIO_CROPPING,
                    0,
                    0,
                    0,
                    0,
                    MWCAP_VIDEO_COLOR_FORMAT_YUV2020,
                    MWCAP_VIDEO_QUANTIZATION_UNKNOWN,
                    MWCAP_VIDEO_SATURATION_UNKNOWN);
                break;
            case MWCAP_VIDEO_COLOR_FORMAT_RGB:
                MWCaptureVideoFrameToVirtualAddressEx(hChannel,
                    VideoBufferInfo.iNewestBuffering,
                    pucFrmBuf, dwImageSize, dwMinStride,
                    0,
                    0,
                    MWFOURCC_BGR24,
                    capWidth,
                    capHeight,
                    0,
                    NUM_PARTIAL_LINE,
                    0,
                    0,
                    0,
                    100,
                    0,
                    100,
                    0,
                    MWCAP_VIDEO_DEINTERLACE_BLEND,
                    MWCAP_VIDEO_ASPECT_RATIO_CROPPING,
                    0,
                    0,
                    0,
                    0,
                    MWCAP_VIDEO_COLOR_FORMAT_UNKNOWN,
                    MWCAP_VIDEO_QUANTIZATION_UNKNOWN,
                    MWCAP_VIDEO_SATURATION_UNKNOWN);
                break;
            default:
                std::cerr << "Capture colorFormat not supported!\n";
                break;
        }

        do {
            MWWaitEvent(hCaptureEvent, 1000);
        } while ((MWGetVideoCaptureStatus(hChannel, &captureStatus) == MW_SUCCEEDED) && (!captureStatus.bFrameCompleted));

        memcpy(frameBuf, pucFrmBuf,dwImageSize);
        break;
    }

    return;
}

// ---------------------------------------------------------------------------
// Zero-copy capture: DMA into the internal buffer and return its pointer
// directly, skipping the memcpy in read().  On the Jetson path pucFrmBuf is
// cudaMallocHost'd, so the GPU encoder can read it without a PCIe upload.
// ---------------------------------------------------------------------------
unsigned char* VideoCapture::readDirect() {

    while(1) {
        if (MWWaitEvent(hNotifyEvent, 1000) <= 0){
            continue;
        }
        if ((MWGetNotifyStatus(hChannel, hNotify, &ullStatusBits) != MW_SUCCEEDED)){
            continue;
        }
        if (MWGetVideoBufferInfo(hChannel, &VideoBufferInfo) != MW_SUCCEEDED){
            continue;
        }
        if (MWGetVideoFrameInfo(hChannel, VideoBufferInfo.iNewestBuffering, &VideoFrameInfo) != MW_SUCCEEDED){
            continue;
        }

#ifdef USE_GPUDIRECT_RDMA
        if (gpuDirectReady) {
            // Capture directly into GPU VRAM via physical address RDMA.
            MWCaptureVideoFrameToPhysicalAddressEx(hChannel,
                VideoBufferInfo.iNewestBuffering,
                gpuPhysAddr, dwImageSize, dwMinStride,
                0,
                0,
                uiFourCC,
                capWidth,
                capHeight,
                0,
                NUM_PARTIAL_LINE,
                0,
                0,
                0,
                100,
                0,
                100,
                0,
                MWCAP_VIDEO_DEINTERLACE_BLEND,
                MWCAP_VIDEO_ASPECT_RATIO_CROPPING,
                0,
                0,
                0,
                0,
                MWCAP_VIDEO_COLOR_FORMAT_YUV2020,
                MWCAP_VIDEO_QUANTIZATION_UNKNOWN,
                MWCAP_VIDEO_SATURATION_UNKNOWN);
        } else
#endif
        {
            // Jetson zero-copy or vanilla path: capture into pucFrmBuf.
            switch (capColorFormat) {
                case MWCAP_VIDEO_COLOR_FORMAT_YUV2020:
                    MWCaptureVideoFrameToVirtualAddressEx(hChannel,
                        VideoBufferInfo.iNewestBuffering,
                        pucFrmBuf, dwImageSize, dwMinStride,
                        0,
                        0,
                        uiFourCC,
                        capWidth,
                        capHeight,
                        0,
                        NUM_PARTIAL_LINE,
                        0,
                        0,
                        0,
                        100,
                        0,
                        100,
                        0,
                        MWCAP_VIDEO_DEINTERLACE_BLEND,
                        MWCAP_VIDEO_ASPECT_RATIO_CROPPING,
                        0,
                        0,
                        0,
                        0,
                        MWCAP_VIDEO_COLOR_FORMAT_YUV2020,
                        MWCAP_VIDEO_QUANTIZATION_UNKNOWN,
                        MWCAP_VIDEO_SATURATION_UNKNOWN);
                    break;
                case MWCAP_VIDEO_COLOR_FORMAT_RGB:
                    MWCaptureVideoFrameToVirtualAddressEx(hChannel,
                        VideoBufferInfo.iNewestBuffering,
                        pucFrmBuf, dwImageSize, dwMinStride,
                        0,
                        0,
                        MWFOURCC_BGR24,
                        capWidth,
                        capHeight,
                        0,
                        NUM_PARTIAL_LINE,
                        0,
                        0,
                        0,
                        100,
                        0,
                        100,
                        0,
                        MWCAP_VIDEO_DEINTERLACE_BLEND,
                        MWCAP_VIDEO_ASPECT_RATIO_CROPPING,
                        0,
                        0,
                        0,
                        0,
                        MWCAP_VIDEO_COLOR_FORMAT_UNKNOWN,
                        MWCAP_VIDEO_QUANTIZATION_UNKNOWN,
                        MWCAP_VIDEO_SATURATION_UNKNOWN);
                    break;
                default:
                    std::cerr << "Capture colorFormat not supported!\n";
                    return nullptr;
            }
        }

        do {
            MWWaitEvent(hCaptureEvent, 1000);
        } while ((MWGetVideoCaptureStatus(hChannel, &captureStatus) == MW_SUCCEEDED) && (!captureStatus.bFrameCompleted));

#ifdef USE_GPUDIRECT_RDMA
        if (gpuDirectReady) {
            // The frame lives in GPU VRAM.  Copy it back to CPU for the
            // current pipeline.  A full integration would wrap gpuDevPtr as a
            // GstCudaMemory buffer and feed it to the encoder directly.
            cudaMemcpy(pucFrmBuf, gpuDevPtr, dwImageSize, cudaMemcpyDeviceToHost);
        }
#endif
        return pucFrmBuf;
    }

    return nullptr;
}
