#ifndef __QCAP2_DRMBUF_H__
#define __QCAP2_DRMBUF_H__

#include "qcap2.h"

struct qcap2_drmbuf_t;

struct qcap2_drmbuf_t {
	int dmabuf_fd;
	size_t size;

	uint32_t handle;
	uint32_t format;
	uint32_t width;
	uint32_t height;
	uint32_t pitch;

	void* pVirAddr;
	uint64_t offset;
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// qcap2_rcbuffer_t
QRESULT qcap2_rcbuffer_set_drmbuf(qcap2_rcbuffer_t* pRCBuffer, qcap2_drmbuf_t* pDRMBuf);
QRESULT qcap2_rcbuffer_get_drmbuf(qcap2_rcbuffer_t* pRCBuffer, qcap2_drmbuf_t** ppDRMBuf);
QRESULT qcap2_rcbuffer_alloc_drmbuf(qcap2_rcbuffer_t* pRCBuffer, int drm_fd, uint32_t format, int width, int height);
QRESULT qcap2_rcbuffer_free_drmbuf(qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_rcbuffer_map_drmbuf(qcap2_rcbuffer_t* pRCBuffer, int nProt);
QRESULT qcap2_rcbuffer_unmap_drmbuf(qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_rcbuffer_alloc_mapped_drmbuf(qcap2_rcbuffer_t* pRCBuffer, int drm_fd, uint32_t format, int width, int height, int nProt);
QRESULT qcap2_rcbuffer_free_mapped_drmbuf(qcap2_rcbuffer_t* pRCBuffer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_DRMBUF_H__
