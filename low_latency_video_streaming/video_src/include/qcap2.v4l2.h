#ifndef __QCAP2_V4L2_H__
#define __QCAP2_V4L2_H__

#include "qcap2.h"

#include <linux/videodev2.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// qcap2_video_source_t
void qcap2_video_source_set_v4l2_name(qcap2_video_source_t* pThis, const char* strName);
const char* qcap2_video_source_get_v4l2_name(qcap2_video_source_t* pThis);
void qcap2_video_source_set_buf_type(qcap2_video_source_t* pThis, v4l2_buf_type nBufType);
void qcap2_video_source_set_memory(qcap2_video_source_t* pThis, v4l2_memory nMemory);
void qcap2_video_source_set_exp_buf(qcap2_video_source_t* pThis, bool bExpBuf);
QRESULT qcap2_video_source_get_fd(qcap2_video_source_t* pThis, int* pFd); // call close() to release fd
void qcap2_video_source_set_v4l2_sg_name(qcap2_video_source_t* pThis, int index, const char* strName);
const char* qcap2_video_source_get_v4l2_sg_name(qcap2_video_source_t* pThis, int index);

// qcap2_video_sink_t
void qcap2_video_sink_set_v4l2_name(qcap2_video_sink_t* pThis, const char* strName);
const char* qcap2_video_sink_get_v4l2_name(qcap2_video_sink_t* pThis);
void qcap2_video_sink_set_buf_type(qcap2_video_sink_t* pThis, v4l2_buf_type nBufType);
void qcap2_video_sink_set_memory(qcap2_video_sink_t* pThis, v4l2_memory nMemory);
QRESULT qcap2_video_sink_get_fd(qcap2_video_sink_t* pThis, int* pFd); // call close() to release fd

// qcap2_video_encoder_t
void qcap2_video_encoder_set_v4l2_name(qcap2_video_encoder_t* pThis, const char* strName);
const char* qcap2_video_encoder_get_v4l2_name(qcap2_video_encoder_t* pThis);
void qcap2_video_encoder_set_buf_type(qcap2_video_encoder_t* pThis, v4l2_buf_type nBufType);
void qcap2_video_encoder_set_memory(qcap2_video_encoder_t* pThis, v4l2_memory nMemory);

// qcap2_audio_source_t
void qcap2_audio_source_set_v4l2_name(qcap2_audio_source_t* pThis, const char* strName);

// qcap2_audio_sink_t
void qcap2_audio_sink_set_v4l2_name(qcap2_audio_sink_t* pThis, const char* strName);
const char* qcap2_audio_sink_get_v4l2_name(qcap2_audio_sink_t* pThis);
void qcap2_audio_sink_set_buf_type(qcap2_audio_sink_t* pThis, v4l2_buf_type nBufType);
void qcap2_audio_sink_set_memory(qcap2_audio_sink_t* pThis, v4l2_memory nMemory);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_V4L2_H__
