cmake_minimum_required(VERSION 3.10)
project(GStreamerVideoStreaming)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# GPU zero-copy options. At most one should be ON.
#
#   USE_JETSON_ZEROCOPY  – Jetson (integrated GPU, unified memory). Capture
#                          buffers are allocated with cudaMallocHost so the
#                          encoder reads them without a PCIe upload.
#
#   USE_GPUDIRECT_RDMA   – Discrete GPU with GPUDirect RDMA. The Magewell
#                          card DMAs directly into GPU VRAM via the
#                          NvRdmaForProCapture kernel module.
# ---------------------------------------------------------------------------
option(USE_JETSON_ZEROCOPY "CUDA pinned memory for zero-copy on Jetson"  ON)
option(USE_GPUDIRECT_RDMA  "GPUDirect RDMA for discrete GPUs"           OFF)

if(USE_JETSON_ZEROCOPY OR USE_GPUDIRECT_RDMA)
    find_package(CUDA REQUIRED)
    include_directories(${CUDA_INCLUDE_DIRS})
    if(USE_JETSON_ZEROCOPY)
        add_definitions(-DUSE_JETSON_ZEROCOPY)
        message(STATUS "Jetson zero-copy enabled (cudaMallocHost capture buffers, GPU stitch/crop)")
    endif()
    if(USE_GPUDIRECT_RDMA)
        add_definitions(-DUSE_GPUDIRECT_RDMA)
        message(STATUS "GPUDirect RDMA enabled (capture to GPU physical address)")
    endif()
    # GPU stitch/crop kernels — compiled as a static library that links
    # into the server.  CUDA_ADD_LIBRARY handles .cu → nvcc compilation.
    cuda_add_library(gpu_stitch STATIC
        ${CMAKE_SOURCE_DIR}/video_src/gpu_stitch.cu
    )
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0>=1.16 gstreamer-app-1.0)
pkg_check_modules(GST_PLUGINS_BASE REQUIRED gstreamer-plugins-base-1.0>=1.16)
# GIO provides the sockets for the signalling/keying channel and the TLS layer
# that authenticates it. Part of glib, so this is not a new package to install.
pkg_check_modules(GIO REQUIRED gio-2.0)

find_package(OpenCV REQUIRED)

set(Magwell_SDK_DIR ${CMAKE_SOURCE_DIR}/third-party/Magewell_SDK)
set(Magwell_SDK_LIB ${Magwell_SDK_DIR}/Lib/arm64/libMWCapture.a)
set(VIDEO_SRC_DIR ${CMAKE_SOURCE_DIR}/video_src)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GST_PLUGINS_BASE_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${Magwell_SDK_DIR}/Include
    ${VIDEO_SRC_DIR}
    ${OpenCV_INCLUDE_DIRS}
)

# Link directories
link_directories(
    ${GSTREAMER_LIBRARY_DIRS}
    ${GST_PLUGINS_BASE_LIBRARY_DIRS}
    ${GIO_LIBRARY_DIRS}
)

# Add compiler flags
add_compile_options(${GSTREAMER_CFLAGS_OTHER})

# Server executable
add_executable(server
               server.cpp
               ${VIDEO_SRC_DIR}/utils.cpp
               ${VIDEO_SRC_DIR}/videoCapture.cpp
               ${VIDEO_SRC_DIR}/videoPipeline.cpp
               ${VIDEO_SRC_DIR}/videoPipelineApi.cpp
)
target_link_libraries(server
    ${GSTREAMER_LIBRARIES}
    ${GST_PLUGINS_BASE_LIBRARIES}
    ${GIO_LIBRARIES}
    ${Magwell_SDK_LIB}
    ${OpenCV_LIBRARIES}
    pthread udev asound v4l2
)
if(USE_JETSON_ZEROCOPY OR USE_GPUDIRECT_RDMA)
    target_link_libraries(server gpu_stitch ${CUDA_LIBRARIES} ${CUDA_CUDART_LIBRARY})
endif()

# Client executable
add_executable(client client.cpp)
target_link_libraries(client
    ${GSTREAMER_LIBRARIES}
    ${GST_PLUGINS_BASE_LIBRARIES}
    ${GIO_LIBRARIES}
)

# ---------------------------------------------------------------------------
# WebRTC transport. Built only when the WebRTC development files are present, so
# the UDP targets above still build on a machine without them.
#
#   sudo apt-get install -y libgstreamer-plugins-bad1.0-dev gstreamer1.0-nice
#
# gstreamer1.0-nice is a runtime requirement rather than a build one: webrtcbin
# instantiates nicesrc/nicesink for ICE, and negotiation fails without them.
# ---------------------------------------------------------------------------
pkg_check_modules(GST_WEBRTC gstreamer-webrtc-1.0>=1.16)
pkg_check_modules(GST_SDP gstreamer-sdp-1.0>=1.16)

if(GST_WEBRTC_FOUND AND GST_SDP_FOUND)
    message(STATUS "WebRTC support enabled (gstreamer-webrtc ${GST_WEBRTC_VERSION})")

    foreach(webrtc_target webrtc_server webrtc_client)
        add_executable(${webrtc_target} ${webrtc_target}.cpp)
        target_include_directories(${webrtc_target} PRIVATE
            ${GSTREAMER_INCLUDE_DIRS}
            ${GST_PLUGINS_BASE_INCLUDE_DIRS}
            ${GST_WEBRTC_INCLUDE_DIRS}
            ${GST_SDP_INCLUDE_DIRS}
            ${GIO_INCLUDE_DIRS}
        )
        target_link_libraries(${webrtc_target}
            ${GSTREAMER_LIBRARIES}
            ${GST_PLUGINS_BASE_LIBRARIES}
            ${GST_WEBRTC_LIBRARIES}
            ${GST_SDP_LIBRARIES}
            ${GIO_LIBRARIES}
        )
    endforeach()

    install(TARGETS webrtc_server webrtc_client DESTINATION bin)
else()
    message(STATUS "WebRTC support disabled: install "
                   "libgstreamer-plugins-bad1.0-dev and gstreamer1.0-nice")
endif()

# Install targets
install(TARGETS server client DESTINATION bin)
