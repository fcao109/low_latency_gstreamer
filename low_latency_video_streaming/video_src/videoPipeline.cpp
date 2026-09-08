#include <thread>

#include "server.h"
#include "utils.h"
#include "videoPipeline.h"
//#include "configManager.h"


// For self test purpose
void runSrcDummy() {

    std::cout << "Run into src dummy" << std::endl;
    unsigned int frameCnt = 0;

    while (1) {

        sleepForSeconds(1);
        frameCnt += 1;
        if (frameCnt % 5 == 0) {
            std::cout << "frameCnt = " << frameCnt << std::endl;
        }
    }

}

void VideoPipeline::runCaptureLeft() {
    // Need to use camera name to differentiate left and right channels
    // std::shared_ptr<cv::VideoCapture> cap = camMap["left"];
    std::shared_ptr<VideoCapture> cap = camMap["left"];

    //cv::Mat frame;
    unsigned char* frame;
    if (params.capColorFormat == MWCAP_VIDEO_COLOR_FORMAT_YUV2020) {
        frame = (unsigned char*)malloc(params.captureWidth * params.captureHeight * 3 / 2);
    } else if (params.capColorFormat == MWCAP_VIDEO_COLOR_FORMAT_RGB) {
        frame = (unsigned char*)malloc(params.captureWidth * params.captureHeight * 3);
    }

    int emptyCnt = 0;

    while (!g_atomic_int_get(&(((ServerData *)serverData)->running))) {
        usleep(10000);
    }

    std::cout << "Enter capture left camera loop..." << ",threadid=" << std::this_thread::get_id() << std::endl;

    while (captureRunning && g_atomic_int_get(&(((ServerData *)serverData)->running))) {
        cap->read(frame);

        // if (frame.empty()) {
        if (frame == nullptr) {
            emptyCnt++;
            std::cerr << "Error: get empty frame from camera=" << camNameMap["left"] << ", Coutner=" << emptyCnt << std::endl;
            sleepForSeconds(0.01);
            continue;
        }
#if profilingOverlay
        cv::Point pt(400,150);
        long long tsNow = getCurrentUTCEpochMillis();
        std::string ts = std::to_string(tsNow);
        overlayTimestamp(frame, ts, pt);
#endif
        capQueueLeft->push(frame);
    }
    logMessage("About to release the left camera....");
    cap->release();
    logMessage("Left camera quits capturing, being closed.\n");

}

void VideoPipeline::runCaptureRight() {
    // Need to use camera name to differentiate left and right channels
    // std::shared_ptr<cv::VideoCapture> cap = camMap["right"];
    std::shared_ptr<VideoCapture> cap = camMap["right"];

    // cv::Mat frame;
    unsigned char* frame;
    if (params.capColorFormat == MWCAP_VIDEO_COLOR_FORMAT_YUV2020) {
        frame = (unsigned char*)malloc(params.captureWidth * params.captureHeight * 3 / 2);
    } else if (params.capColorFormat == MWCAP_VIDEO_COLOR_FORMAT_RGB) {
        frame = (unsigned char*)malloc(params.captureWidth * params.captureHeight * 3);
    }

    int emptyCnt = 0;

    while (!g_atomic_int_get(&(((ServerData *)serverData)->running))) {
        usleep(10000);
    }

    std::cout << "Enter capture right camera loop..." << ",threadid=" << std::this_thread::get_id() << std::endl;

    while (captureRunning && g_atomic_int_get(&(((ServerData *)serverData)->running))) {
        cap->read(frame);

        // if (frame.empty()) {
        if (frame == nullptr) {
            emptyCnt++;
            std::cerr << "Error: get empty frame from camera=" << camNameMap["right"] << ", Coutner=" << emptyCnt << std::endl;
            sleepForSeconds(0.01);
            continue;
        }
#if profilingOverlay
        cv::Point pt(400, 150);
        long long tsNow = getCurrentUTCEpochMillis();
        std::string ts = std::to_string(tsNow);
        overlayTimestamp(frame, ts, pt);
#endif
        capQueueRight->push(frame);
    }

    logMessage("About to release the right camera....");
    cap->release();
    logMessage("Right camera quits capturing, being closed.");

}

VideoPipeline::VideoPipeline() {
    srcRunning = false;
    renderRunning = false;
    captureRunning = false;
    coderRunning = false;

    //videoStats = std::make_shared<VideoStats>();
    //latencyStats = std::make_shared<LatencyStats>();
    //renderStats = std::make_shared<VideoStats>();

    width = 0;
    height = 0;

    capQueueRight = nullptr;
    capQueueLeft = nullptr;
    bufferQueue = nullptr;
    //cv::moveWindow("Console", -1920, 0);

    params = VideoParameters();
}

bool VideoPipeline::setCamera(
    const char* cameraName1,
    const char* cameraName2,
    const int& cameraIndex1,
    const int& cameraIndex2,
    const int& setWidth,
    const int& setHeight,
    const int& roiWidth,
    const int& roiHeight,
    const int& capColorFormat,
    const int& fps
)
{
    logMessage("VideoPipeline::setCamera, please wait for camera initialization...");

    if (isStereoscopic)
    {
        std::shared_ptr<VideoCapture> cap1 = std::make_shared<VideoCapture>(cameraIndex1, setWidth, setHeight, capColorFormat);
        cap1->open();
        usleep(1000 * 400);
        if (!cap1->isOpened())
            return false;

        std::shared_ptr<VideoCapture> cap2 = std::make_shared<VideoCapture>(cameraIndex2, setWidth, setHeight, capColorFormat);
        cap2->open();
        usleep(1000 * 400);
        if (!cap2->isOpened())
            return false;

        width = roiWidth * 2;
        height = roiHeight;

        std::string camera1(cameraName1);
        std::string camera2(cameraName2);

        camIdxMap[camera1] = cameraIndex1;
        camIdxMap[camera2] = cameraIndex2;

        camNameMap["left"] = camera1;
        camNameMap["right"] = camera2;

        logMessage("open successfully left camera - name: " + camera1 + ", index=" + std::to_string(cameraIndex1));
        logMessage("open successfully right camera - name: " + camera2 + ", index=" + std::to_string(cameraIndex2));

        camMap["left"] = cap1;
        camMap["right"] = cap2;
    } else {
        std::shared_ptr<VideoCapture> cap1 = std::make_shared<VideoCapture>(cameraIndex1, setWidth, setHeight);
        cap1->open();
        if (!cap1->isOpened())
            return false;

        width = roiWidth;
        height = roiHeight;

        std::string camera1(cameraName1);
        camIdxMap[camera1] = cameraIndex1;

        camMap[camera1] = cap1;
    }

    return true;
}

void VideoPipeline::runSrc() {
    bool showOnce = true;
    //videoStats->start();

    unsigned char* frame1;
    unsigned char* frame2;
    unsigned char* canvas;
    unsigned char* resizedCanvas;

    const int numImageBuffer = 50;
    // cv::Mat images[numImageBuffer];
    unsigned char** images;

    // pre-allocated image buffers
#if horizontal3D
    images = allocate_images(params.captureWidth * 2, params.captureHeight, numImageBuffer, params.capColorFormat);
#else
    images = allocate_images(params.captureWidth, params.captureHeight * 2, numImageBuffer, params.capColorFormat);
#endif

    int emptyCnt = 0;
    int frameCnt = 0;

    ((ServerData *)serverData)->capture_frame_cnt = frameCnt;

    //vCoder = std::make_shared<VideoCoder>(params);

    coderRunning = true;
    unsigned int nbytes;
    if (params.capColorFormat == MWCAP_VIDEO_COLOR_FORMAT_YUV2020) {
        nbytes = width * height * 3 / 2;   // YUV420
    } else if (params.capColorFormat == MWCAP_VIDEO_COLOR_FORMAT_RGB) {
        nbytes = params.captureWidth * params.captureHeight * 3;
    }

    // latencyStats->add("runSrce2e");
    // latencyStats->add("preprocessing");
    // latencyStats->add("encodeAndSendPkt");

    unsigned char** crop_images;
    unsigned char* crop_canvas;
    if (params.enable_crop) {
        crop_images = allocate_images(params.cropWidth, params.cropHeight, numImageBuffer, params.capColorFormat);
    }

    while (!g_atomic_int_get(&(((ServerData *)serverData)->running))) {
        usleep(10000);
    }

    // TODO: need to handle non-StereoScopic
    while (srcRunning && g_atomic_int_get(&(((ServerData *)serverData)->running))) {
        frame1 = capQueueLeft->pop();
        frame2 = capQueueRight->pop();

        // if (frame1.empty() || frame2.empty()) {
        if (frame1 == nullptr || frame2 == nullptr) {
            emptyCnt++;
            std::cerr << "Error: runSrc get empty frame Coutner= " << emptyCnt << std::endl;
            sleepForSeconds(0.01);
            continue;
        }

        // latencyStats->start("runSrce2e");
        // latencyStats->start("preprocessing");

        canvas = images[frameCnt % numImageBuffer];

#ifdef COMBO_IMAGES
        stitchFrames(frame1, frame2, params.captureWidth, params.captureHeight, canvas, params.capColorFormat);
#else
        memcpy(canvas, frame1, params.captureWidth * params.captureHeight * 3 / 2);
#endif

        if (params.enable_crop) {
            crop_canvas = crop_images[frameCnt % numImageBuffer];
            cropFrames(canvas, params.captureWidth, params.captureHeight * 2, params.cropX, params.cropY, params.cropWidth, params.cropHeight, crop_canvas);
            canvas = crop_canvas;
            //resizeYuv420(crop_canvas, params.cropWidth, params.cropHeight, canvas, params.captureWidth, params.captureHeight * 2);
        }

        resizedCanvas = canvas;

        if (showOnce) {
            // logMessage("frame width=" + std::to_string(width) + ", height=" + std::to_string(height));
            logMessage("VideoPipeline::runSrc, camera initialization done.");
            showOnce = false;
        }

        frameCnt++;
        // logMessage("frameCnt=" + std::to_string(frameCnt));
        ((ServerData *)serverData)->capture_frame_cnt = frameCnt;
        ((ServerData *)serverData)->frameQueue->push(resizedCanvas);

#ifdef LOCAL_RENDERING
        bufferQueue->push(resizedCanvas);
#endif
    }

    logMessage("VideoPipeline::runSrc, exit the source processing loop");
   /* cap1->release();
    cap2->release();*/

    // if (vCoder) vCoder = nullptr;
    return;

}

void VideoPipeline::start() {

    logMessage("VideoPipeline::start, start the source capturing");
    if (!srcThd || !srcThd->joinable()) {
        srcThd = nullptr;
        captureRunning = true;
        srcRunning = true;
        renderRunning = true;
        coderRunning = true;

        // ConfigManager& confMgr = ConfigManager :: getInstance();
        // confMgr.getConfig(params);

        if (!bufferQueue) {
            bufferQueue = std::make_shared<SyncQueue<unsigned char*>>();
        }

        if (!capQueueLeft) capQueueLeft = std::make_shared<SyncQueue<unsigned char*>>();
        if (!capQueueRight) capQueueRight = std::make_shared<SyncQueue<unsigned char*>>();

#ifdef COMBO_IMAGES
#if horizontal3D
        ((ServerData *)serverData)->width = params.captureWidth * 2;
        ((ServerData *)serverData)->height = params.captureHeight;
#else
        ((ServerData *)serverData)->width = params.captureWidth;
        ((ServerData *)serverData)->height = params.captureHeight * 2;
#endif
#else
        ((ServerData *)serverData)->width = params.captureWidth;
        ((ServerData *)serverData)->height = params.captureHeight;
#endif

        srcThd = std::make_shared<std::thread>(&VideoPipeline::runSrc, this);
        capThdLeft = std::make_shared<std::thread>(&VideoPipeline::runCaptureLeft, this);
        capThdRight = std::make_shared<std::thread>(&VideoPipeline::runCaptureRight, this);
    }

}

void VideoPipeline::stop() {

    if (!srcThd || srcRunning == false) return;

    logMessage("VideoPipeline::stop, stop the source capturing");
    srcRunning = false;
    captureRunning = false;
    renderRunning = false;

    sleepForSeconds(1);
    camMap["left"] = nullptr;
    camMap["right"] = nullptr;

    //videoStats = nullptr;
    //latencyStats = nullptr;

    camNameMap.clear();
    camIdxMap.clear();
    camMap.clear();

    capThdLeft->join();
    capThdRight->join();
    srcThd->join();

    srcThd = nullptr;
    capThdLeft = nullptr;
    capThdRight = nullptr;

    bufferQueue = nullptr;
    capQueueLeft = nullptr;
    capQueueRight = nullptr;
}

void VideoPipeline::reload() {
    return;
}

void VideoPipeline::render() {
    cv::Mat buff;
    std::cout << "Enter local render loop..." << ",threadid=" << std::this_thread::get_id() << std::endl;
    logMessage("VideoPipeline::render, start local renderring");
    while (renderRunning) {
        unsigned char* rawbuff = getBuffer();

        if (params.capColorFormat == MWCAP_VIDEO_COLOR_FORMAT_YUV2020) {
#if horizontal3D
            buff = convertYUV420Frame(rawbuff, params.captureWidth * 2, params.captureHeight);
#else
            if (params.enable_crop) {
                buff = convertYUV420Frame(rawbuff, params.cropWidth, params.cropHeight);
            } else {
                buff = convertYUV420Frame(rawbuff, params.captureWidth, params.captureHeight * 2);
            }
#endif
        } else if (params.capColorFormat == MWCAP_VIDEO_COLOR_FORMAT_RGB) {
            cv::Mat mat_rgb(params.captureHeight, params.captureWidth, CV_8UC3, (void*)rawbuff);
            cv::Mat mat_bgr;
            cv::cvtColor(mat_rgb, buff, cv::COLOR_RGB2BGR);
        }

        if (!buff.empty()) {
            // renderStats->measureStats(buff.cols * buff.rows * 3);
            cv::imshow("console2", buff);
            cv::waitKey(1);
        }
    }
    logMessage("VideoPipeline::render, stop local renderring");
    cv::destroyAllWindows();
}

// cv::Mat VideoPipeline::getBuffer() {
unsigned char* VideoPipeline::getBuffer() {
    //cv::Mat buff;
    unsigned char* buff = nullptr;
    if (bufferQueue) {
        buff = bufferQueue->pop();
    }
    return buff;
}

int VideoPipeline::getBufferCapacity() {
    if (bufferQueue) {
        //return bufferQueue->getCapapcity();
        return 1;
    }
    return 0;
}

#if 0
double VideoPipeline::getVideoFPS() {
    return videoStats->getFps();
}

double VideoPipeline::getVideoBandwidth() {
    return videoStats->getBandwidth();
}

double VideoPipeline::getRenderFPS() {
    return renderStats->getFps();
}

double VideoPipeline::getRenderBandwidth() {
    return renderStats->getBandwidth();
}

void VideoPipeline::showPerformanceStats() {
    latencyStats->showStats();
    if (vCoder && coderRunning) {
        vCoder->showStats();
    }

#ifdef LOCAL_RENDERING
    std::cout << "--- Render Stats ---" << std::endl;
    renderStats->showStats();
#endif
}


std::string VideoPipeline::getVideoCodec() const {
    if (vCoder && coderRunning) {
        return vCoder->getCodec();
    }
    return "Not running/configured";
}

bool VideoPipeline::setVideoCodec(const std::string& codec_name) {
    if (!vCoder) {
        vCoder = std::make_shared<VideoCoder>(codec_name, 4.0, 30);
    }
    colorDepth = 888;
    scalingFactor = 1.0;
    //bufferQueue->setCapacity(1);
    coderRunning = true;
    return vCoder->setCodec(codec_name);
}

double VideoPipeline::getVideoBitrate() const {
    if (vCoder) {
        return vCoder->getBitRate();
    }
    std::cerr << "video encoder/decoder not setup yet\n";
    return 0.0;
}

bool VideoPipeline::setVideoBitrate(const double& video_bitrate) {
    if (vCoder) {
        return vCoder->setBitRate(video_bitrate);
    }
    std::cerr << "video encoder/decoder not setup/enabled yet\n";
    return false;
}

bool VideoPipeline::setH264Preset(const std::string& preVal) {
    if (vCoder) {
        return vCoder->setH264Preset(preVal);
    }
    std::cerr << "video encoder/decoder not setup/enabled yet\n";
    return false;
}

bool VideoPipeline::setH264Crf(const std::string& crfVal) {
    if (vCoder) {
        return vCoder->setH264Crf(crfVal);
    }
    std::cerr << "video encoder/decoder not setup/enabled yet\n";
    return false;
}

bool VideoPipeline::setH264Gop(const int& gopVal) {
    if (vCoder) {
        return vCoder->setH264Gop(gopVal);
    }
    std::cerr << "video encoder/decoder not setup/enabled yet\n";
    return false;
}

std::string VideoPipeline::getH264Preset() const {
    if (vCoder) {
        return vCoder->getH264Preset();
    }
    return "";
}

std::string VideoPipeline::getH264Crf() const {
    if (vCoder) {
        return vCoder->getH264Crf();
    }
    return "";
}

int VideoPipeline::getH264Gop() const {
    if (vCoder) {
        return vCoder->getH264Gop();
    }
    return -1;
}
#endif