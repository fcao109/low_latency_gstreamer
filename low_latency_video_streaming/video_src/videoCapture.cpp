#include "videoCapture.h"

#define NUM_PARTIAL_LINE 64

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

    pucFrmBuf = (unsigned char*)malloc(dwImageSize);
    if(nullptr == pucFrmBuf){
        std::cerr << "Allocation of pucFrmBuf fails\n";
        return -1;
    }

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

    MWPinVideoBuffer(hChannel, (MWCAP_PTR)pucFrmBuf, dwImageSize);
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

    // if(render){
    //     delete render;
    // }
    // sem_destroy(&g_Sem);
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
