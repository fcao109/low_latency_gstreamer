#ifndef __QCAP2_GST_H__
#define __QCAP2_GST_H__

#include "qcap2.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// qcap2_video_sink_t
void qcap2_video_sink_set_gst_sink_name(qcap2_video_sink_t* pThis, const char* strGstSinkName);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_GST_H__