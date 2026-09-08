#ifndef __QCAP2_USER_H__
#define __QCAP2_USER_H__

#include "qcap2.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// qcap2_rcbuffer_t
qcap2_rcbuffer_t* qcap2_rcbuffer_new_av_frame();
qcap2_rcbuffer_t* qcap2_rcbuffer_new_av_packet();

// qcap2_video_source_t
QRESULT qcap2_video_source_fire_event(qcap2_video_source_t* pThis, int n);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_USER_H__