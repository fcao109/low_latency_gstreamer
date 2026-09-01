#include "utils.h"
#include "LibMWCapture/MWCaptureExtension.h"

 long long getCurrentUTCEpochMillis() {
       auto now = std::chrono::system_clock::now();
       auto duration = now.time_since_epoch();
       return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

bool rectifyRoi(cv::Rect& roi, cv::Mat& img) {
    return (roi.x >= 0) && (roi.y >= 0) && (roi.x + roi.width <= img.cols) && (roi.y + roi.height <= img.rows);
}

void sleepForSeconds(double seconds) {
#if defined(_WIN32) || defined(WIN32)
        Sleep(seconds);
#else
        usleep(1000000 * seconds);
#endif
}

void overlayTimestamp(cv::Mat &frame, std::string& ts, cv::Point& pos) {
       //long long timestamp = getCurrentUTCEpochMillis();
       //std::string text = std::to_string(timestamp);
       int fontFace = cv::FONT_HERSHEY_SIMPLEX;
       double fontScale = 1;
       int thickness = 2;
       //cv::Point textOrg(10, 50); // Position of the text

       cv::putText(frame, ts, pos, fontFace, fontScale, cv::Scalar::all(255), thickness, 8);
}

void stitchCVFrames(cv::Mat& image1, cv::Mat& image2, cv::Mat& canvas) {

    // Center crop the frame to 1280x1024
    cv::Mat cropped1, cropped2;

    if (image1.cols > 1280) {
        cv::Rect roi1(int((image1.cols - 1280) / 2), int((image1.rows - 1024) / 2), 1280, 1024);
        cv::Rect roi2(int((image2.cols - 1280) / 2), int((image2.rows - 1024) / 2), 1280, 1024);


        cropped1 = image1(roi1);
        cropped2 = image2(roi2);
    }
    else {
        cropped1 = image1;
        cropped2 = image2;
    }
#if horizontal3D
    cv::Rect roi_left = cv::Rect(0, 0, cropped1.cols, cropped1.rows);
    cropped1.copyTo(canvas(roi_left));
    cv::Rect roi_right = cv::Rect(cropped1.cols, 0, cropped2.cols, cropped2.rows);
    cropped2.copyTo(canvas(roi_right));
#else
    cv::Rect roi_top = cv::Rect(0, 0, cropped1.cols, cropped1.rows);
    cropped1.copyTo(canvas(roi_top));
    cv::Rect roi_bottom = cv::Rect(0, cropped1.rows, cropped2.cols, cropped2.rows);
    cropped2.copyTo(canvas(roi_bottom));
#endif

    // cv::hconcat(cropped1, cropped2, canvas);
    //return canvas;
    return;
}

void cropFrames(unsigned char* srcBuffer,
                int srcWidth, int srcHeight,
                int cropX, int cropY, int cropWidth, int cropHeight,
                unsigned char* destBuffer) {

    // Crop coordinates must be even for YUV420 to keep chroma alignment
    if (cropX % 2 != 0 || cropY % 2 != 0) {
        std::cerr << "Crop X and Y must be even numbers." << std::endl;
        return;
    }

    size_t ySize = cropWidth * cropHeight;
    size_t uvSize = (cropWidth / 2) * (cropHeight / 2);

    // Crop Y Plane
    const unsigned char* srcY = srcBuffer;
    unsigned char* destY = destBuffer;
    for (int r = 0; r < cropHeight; ++r) {
        int srcRowIdx = (cropY + r) * srcWidth + cropX;
        std::memcpy(destY + (r * cropWidth), srcY + srcRowIdx, cropWidth);
    }

    // Crop U & V Planes
    const unsigned char* srcU = srcBuffer + (srcWidth * srcHeight);
    const unsigned char* srcV = srcU + ((srcWidth / 2) * (srcHeight / 2));

    unsigned char* destU = destY + ySize;
    unsigned char* destV = destU + uvSize;

    int uvCropWidth = cropWidth / 2;
    int uvCropHeight = cropHeight / 2;
    int srcUVStride = srcWidth / 2;
    int destUVStride = cropWidth / 2;

    for (int r = 0; r < uvCropHeight; ++r) {
        int srcUvRowIdx = ((cropY / 2) + r) * srcUVStride + (cropX / 2);

        std::memcpy(destU + (r * destUVStride), srcU + srcUvRowIdx, uvCropWidth);
        std::memcpy(destV + (r * destUVStride), srcV + srcUvRowIdx, uvCropWidth);
    }
}

// need to preallocate combined_frame to be width*2 x height*2
void stitchFrames(unsigned char* frame1, unsigned char* frame2, int width, int height, unsigned char* combined_frame, int capColorFormat) {
    // printf("%s: width %d, height %d, capColorFormat %d\n", __func__, width, height, capColorFormat);
    if ((MWCAP_VIDEO_COLOR_FORMAT)capColorFormat == MWCAP_VIDEO_COLOR_FORMAT_YUV2020) {
        int y_size = width * height;
        int uv_size = (width / 2) * (height / 2);

#if horizontal3D
        // Combined frame's dimensions
        int combined_width = width * 2;

        // Pointer offsets in the combined frame
        unsigned char *y_dst = combined_frame;
        unsigned char *u_dst = combined_frame + combined_width * height;
        unsigned char *v_dst = combined_frame + combined_width * height + (combined_width / 2) * (height / 2);

        // Y Plane: Interleave first and second frames
        for (int row = 0; row < height; row++) {
            memcpy(y_dst + row * combined_width, frame1 + row * width, width);        // Left part (first frame)
            memcpy(y_dst + row * combined_width + width, frame2 + row * width, width); // Right part (second frame)
        }

        // U Plane: Interleave first and second frames
        for (int row = 0; row < height / 2; row++) {
            memcpy(u_dst + row * (combined_width / 2), frame1 + y_size + row * (width / 2), width / 2);        // Left part (first frame)
            memcpy(u_dst + row * (combined_width / 2) + (width / 2), frame2 + y_size + row * (width / 2), width / 2); // Right part (second frame)
        }

        // V Plane: Interleave first and second frames
        for (int row = 0; row < height / 2; row++) {
            memcpy(v_dst + row * (combined_width / 2), frame1 + y_size + uv_size + row * (width / 2), width / 2);        // Left part (first frame)
            memcpy(v_dst + row * (combined_width / 2) + (width / 2), frame2 + y_size + uv_size + row * (width / 2), width / 2); // Right part (second frame)
        }
#else
        // Copy Y planes
        memcpy(combined_frame, frame1, y_size);
        memcpy(combined_frame + y_size, frame2, y_size);

        // Copy U planes
        memcpy(combined_frame + 2 * y_size, frame1 + y_size, uv_size);
        memcpy(combined_frame + 2 * y_size + uv_size, frame2 + y_size, uv_size);

        // Copy V planes
        memcpy(combined_frame + 2 * y_size + 2 * uv_size, frame1 + y_size + uv_size, uv_size);
        memcpy(combined_frame + 2 * y_size + 3 * uv_size, frame2 + y_size + uv_size, uv_size);
#endif
    } else if ((MWCAP_VIDEO_COLOR_FORMAT)capColorFormat == MWCAP_VIDEO_COLOR_FORMAT_RGB) {
#if 0
        int channels = 3; // RGB24
        int row_bytes_single = width * channels;
        int row_bytes_output = width * 2 * channels;

        for (int y = 0; y < height; y++) {
            // Destination pointers for the current row in the output image
            unsigned char *out_row_left  = combined_frame + (y * row_bytes_output);
            unsigned char *out_row_right = combined_frame + (y * row_bytes_output) + row_bytes_single;

            // Source pointers for the input images
            const unsigned char *in_row_left  = frame1 + (y * row_bytes_single);
            const unsigned char *in_row_right = frame2 + (y * row_bytes_single);

            // Copy row data
            memcpy(out_row_left, in_row_left, row_bytes_single);
            memcpy(out_row_right, in_row_right, row_bytes_single);
        }
#else
        memcpy(combined_frame, frame1, width * height * 3);
#endif
    }
}

// check 3-digit color depth
bool isColordepthCorrect(const int& colorDepth) {
    int r, g, b;
    r = colorDepth / 100;
    g = int(colorDepth / 10) % 10;
    b = colorDepth % 10;
    if (r < 1 || r > 8 || g < 1 || g > 8 || b < 1 || b > 8) return false;
    return true;
}


// Heavy operation, only for demo purpose
cv::Mat reduceBitDepth(const cv::Mat& img, int bitDepth) {
    cv::Mat reducedImg = img.clone();
    int shiftR = 8 - bitDepth/100;
    int maskR = (1 << (bitDepth/100)) - 1;
    maskR <<= shiftR;
    int shiftG = 8 - ((bitDepth / 10) % 10);
    int maskG = (1 << ((bitDepth / 10) % 10)) - 1;
    maskG <<= shiftG;
    int shiftB = (8 - (bitDepth % 10));
    int maskB = (1 << (bitDepth % 10)) - 1;
    maskB <<= shiftB;

    for (int y = 0; y < img.rows; ++y) {
        for (int x = 0; x < img.cols; ++x) {
            cv::Vec3b& pixel = reducedImg.at<cv::Vec3b>(y, x);
            pixel[0] = (pixel[0] >> shiftB) << shiftB & maskB; // Blue channel
            pixel[1] = (pixel[1] >> shiftG) << shiftG & maskG; // Green channel
            pixel[2] = (pixel[2] >> shiftR) << shiftR & maskR; // Red channel
        }
    }

    return reducedImg;
}

unsigned char** allocate_images(int width, int height, int num_images, int capColorFormat) {
    int image_size;
    if ((MWCAP_VIDEO_COLOR_FORMAT)capColorFormat == MWCAP_VIDEO_COLOR_FORMAT_YUV2020) {
        image_size = (width * height * 3) / 2;  // Size of one YUV 4:2:0 image
    } else if ((MWCAP_VIDEO_COLOR_FORMAT)capColorFormat == MWCAP_VIDEO_COLOR_FORMAT_RGB) {
        image_size = (width * height * 3);
    }
    unsigned char **images = (unsigned char **)malloc(num_images * sizeof(unsigned char*));
    if (images == NULL) {
        fprintf(stderr, "Memory allocation failed for image pointers.\n");
        return NULL;
    }
    for (int i = 0; i < num_images; i++) {
        images[i] = (unsigned char*)malloc(image_size);
        if (images[i] == NULL) {
            fprintf(stderr, "Memory allocation failed for image %d.\n", i);
            // Free already allocated memory on failure
            for (int j = 0; j < i; j++) {
                free(images[j]);
            }
            free(images);
            return NULL;
        }
    }

    return images;
}

// Function to convert YUV420 to BGR and display using OpenCV
cv::Mat convertYUV420Frame(unsigned char* yuvData, int width, int height) {
    // printf("%s %d: width=%d, height=%d\n",__func__,__LINE__, width, height);

    // Create a cv::Mat for the Y channel
    cv::Mat yMat(height, width, CV_8UC1, yuvData);

    // Create a cv::Mat for the U and V channels
    cv::Mat uMat(height / 2, width / 2, CV_8UC1, yuvData + width * height);
    cv::Mat vMat(height / 2, width / 2, CV_8UC1, yuvData + width * height + (width / 2) * (height / 2));

    // Resize U and V channels to match the Y channel size
    cv::Mat uMatResized, vMatResized;
    cv::resize(uMat, uMatResized, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);
    cv::resize(vMat, vMatResized, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);

    // Merge Y, U, and V channels into one YUV image
    std::vector<cv::Mat> yuvChannels = { yMat, uMatResized, vMatResized };
    cv::Mat yuvImage;
    cv::merge(yuvChannels, yuvImage);

    // Convert YUV image to BGR format
    cv::Mat bgrImage;
    cv::cvtColor(yuvImage, bgrImage, cv::COLOR_YUV2BGR);

    return bgrImage;
}

void resizeYuv420(unsigned char* srcYuv, int srcWidth, int srcHeight, unsigned char* dstYuv, int dstWidth, int dstHeight) {
    // Map original channels using pointers (Zero-copy)
    cv::Mat srcY(srcHeight, srcWidth, CV_8UC1, srcYuv);
    cv::Mat srcU(srcHeight / 2, srcWidth / 2, CV_8UC1, srcYuv + (srcWidth * srcHeight));
    cv::Mat srcV(srcHeight / 2, srcWidth / 2, CV_8UC1, srcYuv + (srcWidth * srcHeight * 5 / 4));

    // Map destination channels using pointers
    cv::Mat dstY(dstHeight, dstWidth, CV_8UC1, dstYuv);
    cv::Mat dstU(dstHeight / 2, dstWidth / 2, CV_8UC1, dstYuv + (dstHeight * dstHeight));
    cv::Mat dstV(dstHeight / 2, dstWidth / 2, CV_8UC1, dstYuv + (dstHeight * dstHeight * 5 / 4));

    // Resize each plane independently
    cv::resize(srcY, dstY, cv::Size(dstWidth, dstHeight), 0, 0, cv::INTER_LINEAR);
    cv::resize(srcU, dstU, cv::Size(dstWidth / 2, dstHeight / 2), 0, 0, cv::INTER_LINEAR);
    cv::resize(srcV, dstV, cv::Size(dstWidth / 2, dstHeight / 2), 0, 0, cv::INTER_LINEAR);
}