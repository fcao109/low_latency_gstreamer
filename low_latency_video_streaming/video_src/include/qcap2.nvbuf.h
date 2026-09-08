#ifndef __QCAP2_NVBUF_H__
#define __QCAP2_NVBUF_H__

#include "qcap2.h"

#include <nvbufsurface.h>

// NvBufSurfaceColorFormat extension
enum qcap2_nvbuf_color_format_t {
	QCAP2_NVBUF_COLOR_FORMAT_BGRP = 0x1000,
	QCAP2_NVBUF_COLOR_FORMAT_GBRP,
	QCAP2_NVBUF_COLOR_FORMAT_YV16,
	// QCAP2_NVBUF_COLOR_FORMAT_I422,
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// qcap2_rcbuffer_t
QRESULT qcap2_rcbuffer_set_nvbuf(qcap2_rcbuffer_t* pRCBuffer, NvBufSurface* surface);
QRESULT qcap2_rcbuffer_get_nvbuf(qcap2_rcbuffer_t* pRCBuffer, NvBufSurface** pSurface);
QRESULT qcap2_rcbuffer_alloc_nvbuf(qcap2_rcbuffer_t* pRCBuffer, NvBufSurfaceCreateParams* params);
QRESULT qcap2_rcbuffer_free_nvbuf(qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_rcbuffer_map_nvbuf(qcap2_rcbuffer_t* pRCBuffer, NvBufSurfaceMemMapFlags flags);
QRESULT qcap2_rcbuffer_unmap_nvbuf(qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_rcbuffer_map_cures_nvbuf(qcap2_rcbuffer_t* pRCBuffer, unsigned int flags);
QRESULT qcap2_rcbuffer_unmap_cures_nvbuf(qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_rcbuffer_sync_nvbuf_for_device(qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_rcbuffer_sync_nvbuf_for_cpu(qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_rcbuffer_sync_nvbuf_for_cpu1(qcap2_rcbuffer_t* pRCBuffer, bool copy);
QRESULT qcap2_rcbuffer_alloc_mapped_nvbuf(qcap2_rcbuffer_t* pRCBuffer, NvBufSurfaceCreateParams* params, NvBufSurfaceMemMapFlags memMapFlags, unsigned int curesMapflags);
QRESULT qcap2_rcbuffer_free_mapped_nvbuf(qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_rcbuffer_update_nvbuf_for_cpu(qcap2_rcbuffer_t* pRCBuffer, int nLeft, int nTop, int nRight, int nBottom);
QRESULT qcap2_rcbuffer_update_nvbuf_for_device(qcap2_rcbuffer_t* pRCBuffer, int nLeft, int nTop, int nRight, int nBottom);

// qcap2_video_sink_t
void qcap2_video_sink_set_nvbuf(qcap2_video_sink_t* pThis, bool bNVBuf);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_NVBUF_H__