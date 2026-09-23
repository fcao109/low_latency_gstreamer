cmake_minimum_required(VERSION 3.10)
project(GStreamerVideoStreaming)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0>=1.16 gstreamer-app-1.0)
pkg_check_modules(GST_PLUGINS_BASE REQUIRED gstreamer-plugins-base-1.0>=1.16)
# GIO provides the sockets for the signalling/keying channel and the TLS layer
# that authenticates it. Part of glib, so this is not a new package to install.
pkg_check_modules(GIO REQUIRED gio-2.0)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GST_PLUGINS_BASE_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
)

# Link directories
link_directories(
    ${GSTREAMER_LIBRARY_DIRS}
    ${GST_PLUGINS_BASE_LIBRARY_DIRS}
    ${GIO_LIBRARY_DIRS}
)

# Add compiler flags
add_compile_options(${GSTREAMER_CFLAGS_OTHER})

# Client executable
add_executable(client client.cpp)
target_link_libraries(client
    ${GSTREAMER_LIBRARIES}
    ${GST_PLUGINS_BASE_LIBRARIES}
    ${GIO_LIBRARIES}
)

# Install targets
# install(TARGETS client DESTINATION bin)
