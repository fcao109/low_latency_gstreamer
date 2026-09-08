#ifndef __QCAP2_BUFFER_V2_BRIDGE_H__
#define __QCAP2_BUFFER_V2_BRIDGE_H__

#include "qcap2_buffer_v2.h"
#include "qcap2.h"
#include "qcap2.dmabuf.h"
#include "qcap2.drmbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Phase 0 Bidirectional Zero-Copy Adaptors
 *
 * Provides smooth, transparent interoperability between the legacy qcap2_rcbuffer_t
 * (ZzRefCountedBuffer2) and the new qcap2_buffer_v2_t.
 */

/* Wraps a legacy qcap2_rcbuffer_t (sysbuf, dmabuf, drmbuf, cuda, nvbuf) into a qcap2_buffer_v2_t view */
QCAP2_BUFFER_V2_API qcap2_buffer_v2_t* qcap2_buffer_v2_from_rcbuffer(qcap2_rcbuffer_t* pOldRc);

/* Wraps a new qcap2_buffer_v2_t into a legacy qcap2_rcbuffer_t view */
QCAP2_BUFFER_V2_API qcap2_rcbuffer_t* qcap2_rcbuffer_from_buffer_v2(qcap2_buffer_v2_t* pNewBuf);

#ifdef __cplusplus
}
#endif

#endif /* __QCAP2_BUFFER_V2_BRIDGE_H__ */
