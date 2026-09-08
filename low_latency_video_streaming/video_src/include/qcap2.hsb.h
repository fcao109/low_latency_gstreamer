#ifndef __QCAP2_HSB_H__
#define __QCAP2_HSB_H__

#include "qcap2.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// qcap2_video_source_t
void qcap2_video_source_set_device_ordinal(qcap2_video_source_t* pThis, int nDeviceOrdinal);
void qcap2_video_source_set_hololink_ip(qcap2_video_source_t* pThis, const char* strHololinkIP);
void qcap2_video_source_set_ibv_name(qcap2_video_source_t* pThis, const char* strIBVName);
void qcap2_video_source_set_ibv_port(qcap2_video_source_t* pThis, uint32_t nIBVPort);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_HSB_H__