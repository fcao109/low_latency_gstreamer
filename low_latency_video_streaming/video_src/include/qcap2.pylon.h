#ifndef __QCAP2_PYLON_H__
#define __QCAP2_PYLON_H__

#include "qcap2.h"

#include <pylonc/PylonC.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// qcap2_video_source_t
void qcap2_video_source_set_trigger_mode(qcap2_video_source_t* pThis, bool bOn);
QRESULT qcap2_video_source_get_device_handle(qcap2_video_source_t* pThis, PYLON_DEVICE_HANDLE* pDeviceHandle);

#if 1 // DEPRECATED
QRESULT qcap2_video_source_set_offsetx(qcap2_video_source_t* pThis, int nOffsetX);
QRESULT qcap2_video_source_set_offsety(qcap2_video_source_t* pThis, int nOffsetY);
QRESULT qcap2_video_source_set_exposure_time(qcap2_video_source_t* pThis, float fExposureTime);
QRESULT qcap2_video_source_set_white_balance_auto(qcap2_video_source_t* pThis, int nBalanceWhiteAutoEnum);
QRESULT qcap2_video_source_set_auto_gain_lower_limit(qcap2_video_source_t* pThis, float fAutoGainLowerLimit);
QRESULT qcap2_video_source_set_auto_gain_upper_limit(qcap2_video_source_t* pThis, float fAutoGainUpperLimit);
QRESULT qcap2_video_source_set_gain(qcap2_video_source_t* pThis, float fGain);
QRESULT qcap2_video_source_set_gain_auto(qcap2_video_source_t* pThis, int nGainAutoEnum);
QRESULT qcap2_video_source_trigger(qcap2_video_source_t* pThis);
#endif // DEPRECATED

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_PYLON_H__