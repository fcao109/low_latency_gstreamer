#ifndef __QCAP2_DMABUF_H__
#define __QCAP2_DMABUF_H__

#include "qcap2.h"

struct qcap2_dmabuf_t;

struct qcap2_dmabuf_t {
	int fd;
	size_t dmabuf_size;

	void* pVirAddr;
	uintptr_t nPhyAddr;
	size_t nSize;
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// qcap2_rcbuffer_t
QRESULT qcap2_rcbuffer_set_qdmabuf(qcap2_rcbuffer_t* pRCBuffer, qcap2_dmabuf_t* pDMABuf);
QRESULT qcap2_rcbuffer_get_qdmabuf(qcap2_rcbuffer_t* pRCBuffer, qcap2_dmabuf_t** ppDMABuf);
QRESULT qcap2_rcbuffer_alloc_qdmabuf(qcap2_rcbuffer_t* pRCBuffer, int nSize, int nProt);
QRESULT qcap2_rcbuffer_free_qdmabuf(qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_rcbuffer_map_qdmabuf(qcap2_rcbuffer_t* pRCBuffer, int nProt);
QRESULT qcap2_rcbuffer_unmap_qdmabuf(qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_rcbuffer_alloc_mapped_qdmabuf(qcap2_rcbuffer_t* pRCBuffer, int nSize, int nProt);
QRESULT qcap2_rcbuffer_free_mapped_qdmabuf(qcap2_rcbuffer_t* pRCBuffer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_DMABUF_H__