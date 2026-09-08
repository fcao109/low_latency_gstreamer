#ifndef __QCAP2_FRAME_POOL_V2_H__
#define __QCAP2_FRAME_POOL_V2_H__

#include "qcap2_buffer_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque Handle for Frame Pool V2 (Phase 1 Infrastructure) */
struct qcap2_frame_pool_v2;
typedef struct qcap2_frame_pool_v2 qcap2_frame_pool_v2_t;

/* Pool Lifetime Management */
QCAP2_BUFFER_V2_API qcap2_frame_pool_v2_t* qcap2_frame_pool_v2_create(void);
QCAP2_BUFFER_V2_API void qcap2_frame_pool_v2_destroy(qcap2_frame_pool_v2_t* pPool);

/* Configuration Parameters */
QCAP2_BUFFER_V2_API qcap2_status_t qcap2_frame_pool_v2_set_capacity(
    qcap2_frame_pool_v2_t* pPool,
    uint32_t               nCapacity
);

QCAP2_BUFFER_V2_API qcap2_status_t qcap2_frame_pool_v2_set_video_property(
    qcap2_frame_pool_v2_t* pPool,
    uint32_t               nPixelFormat,
    uint32_t               nWidth,
    uint32_t               nHeight
);

QCAP2_BUFFER_V2_API qcap2_status_t qcap2_frame_pool_v2_set_audio_property(
    qcap2_frame_pool_v2_t* pPool,
    uint32_t               nChannels,
    uint32_t               nSampleRate,
    uint32_t               nFrameSize
);

/* State Control & Buffer Acquisition */
QCAP2_BUFFER_V2_API qcap2_status_t qcap2_frame_pool_v2_start(qcap2_frame_pool_v2_t* pPool);
QCAP2_BUFFER_V2_API void           qcap2_frame_pool_v2_stop(qcap2_frame_pool_v2_t* pPool);

QCAP2_BUFFER_V2_API qcap2_status_t qcap2_frame_pool_v2_acquire_buffer(
    qcap2_frame_pool_v2_t* pPool,
    qcap2_buffer_v2_t**    ppBuffer,
    uint32_t               nTimeoutMs
);

/* Information Queries */
QCAP2_BUFFER_V2_API uint32_t qcap2_frame_pool_v2_get_capacity(const qcap2_frame_pool_v2_t* pPool);
QCAP2_BUFFER_V2_API uint32_t qcap2_frame_pool_v2_get_available(const qcap2_frame_pool_v2_t* pPool);

#ifdef __cplusplus
}
#endif

#endif /* __QCAP2_FRAME_POOL_V2_H__ */
