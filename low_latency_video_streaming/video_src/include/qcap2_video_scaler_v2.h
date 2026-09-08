#ifndef __QCAP2_VIDEO_SCALER_V2_H__
#define __QCAP2_VIDEO_SCALER_V2_H__

#include "qcap2_buffer_v2.h"
#include "qcap2_frame_pool_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque Video Scaler V2 Handle */
struct qcap2_video_scaler_v2;
typedef struct qcap2_video_scaler_v2 qcap2_video_scaler_v2_t;

/* Video Scaler Operations */
QCAP2_BUFFER_V2_API qcap2_video_scaler_v2_t* qcap2_video_scaler_v2_create(void);
QCAP2_BUFFER_V2_API void qcap2_video_scaler_v2_destroy(qcap2_video_scaler_v2_t* pScaler);

QCAP2_BUFFER_V2_API qcap2_status_t qcap2_video_scaler_v2_set_output_format(
    qcap2_video_scaler_v2_t* pScaler,
    uint32_t                 nDstFormat,
    uint32_t                 nDstWidth,
    uint32_t                 nDstHeight
);

QCAP2_BUFFER_V2_API qcap2_status_t qcap2_video_scaler_v2_start(qcap2_video_scaler_v2_t* pScaler);
QCAP2_BUFFER_V2_API void           qcap2_video_scaler_v2_stop(qcap2_video_scaler_v2_t* pScaler);

/* Zero-copy / Descriptor-aware scale and format transform */
QCAP2_BUFFER_V2_API qcap2_status_t qcap2_video_scaler_v2_process_frame(
    qcap2_video_scaler_v2_t* pScaler,
    const qcap2_buffer_v2_t* pSrcBuffer,
    qcap2_buffer_v2_t**      ppDstBuffer
);

#ifdef __cplusplus
}
#endif

#endif /* __QCAP2_VIDEO_SCALER_V2_H__ */
