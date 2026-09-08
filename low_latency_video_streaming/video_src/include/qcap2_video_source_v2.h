#ifndef __QCAP2_VIDEO_SOURCE_V2_H__
#define __QCAP2_VIDEO_SOURCE_V2_H__

#include "qcap2_buffer_v2.h"
#include "qcap2_frame_pool_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque Video Source V2 Handle */
struct qcap2_video_source_v2;
typedef struct qcap2_video_source_v2 qcap2_video_source_v2_t;

/* Video Frame Arrival Callback */
typedef void (*qcap2_video_source_v2_callback_fn)(
    qcap2_video_source_v2_t* pSource,
    qcap2_buffer_v2_t*       pFrameBuffer,
    void*                    pUserData
);

/* Video Source Operations */
QCAP2_BUFFER_V2_API qcap2_video_source_v2_t* qcap2_video_source_v2_create(void);
QCAP2_BUFFER_V2_API void qcap2_video_source_v2_destroy(qcap2_video_source_v2_t* pSource);

QCAP2_BUFFER_V2_API qcap2_status_t qcap2_video_source_v2_set_format(
    qcap2_video_source_v2_t* pSource,
    uint32_t                 nPixelFormat,
    uint32_t                 nWidth,
    uint32_t                 nHeight,
    double                   dFrameRate
);

QCAP2_BUFFER_V2_API qcap2_status_t qcap2_video_source_v2_register_callback(
    qcap2_video_source_v2_t*           pSource,
    qcap2_video_source_v2_callback_fn  pCallback,
    void*                              pUserData
);

QCAP2_BUFFER_V2_API qcap2_status_t qcap2_video_source_v2_start(qcap2_video_source_v2_t* pSource);
QCAP2_BUFFER_V2_API void           qcap2_video_source_v2_stop(qcap2_video_source_v2_t* pSource);

/* Simulate frame capture step (for software pipelines & testing) */
QCAP2_BUFFER_V2_API qcap2_status_t qcap2_video_source_v2_produce_frame(qcap2_video_source_v2_t* pSource);

#ifdef __cplusplus
}
#endif

#endif /* __QCAP2_VIDEO_SOURCE_V2_H__ */
