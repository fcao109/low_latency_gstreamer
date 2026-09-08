#ifndef __QCAP_CCVT_H__
#define __QCAP_CCVT_H__

#include "qcap.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

// device custom property
enum {
	QCAP_CCVTPROP_OUTPUT_BUFFER_TYPE = 1000, // [in, ULONG] 0:device-buffer, 1:host-buffer
	QCAP_CCVTPROP_FRAMES_DROPPED = 1001, // [out, ULONG]
	QCAP_CCVTPROP_INTERLEAVE_OUTPUT = 1002, // [out, ULONG] 0:planar, 1:interlaved
};

// CALLBACK FUNCTION TYPES //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
typedef QRETURN (QCAP_EXPORT *PF_VIDEO_CCVT_CALLBACK)( PVOID pCCVT /*IN*/, BYTE * pFrameBuffer /*IN*/, ULONG nFrameBufferLen /*IN*/, PVOID pUserData /*IN*/ );

// CCVT FUNCTIONS /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_CCVT(ULONG nSrcW /*IN*/, ULONG nSrcH /*IN*/, ULONG nSrcColorSpaceType, ULONG nCropX /*IN*/, ULONG nCropY /*IN*/, ULONG nCropW /*IN*/, ULONG nCropH, ULONG nDstW /*IN*/, ULONG nDstH /*IN*/, ULONG nDstColorSpaceType, PVOID* ppCCVT);

QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_DESTROY_CCVT(PVOID pCCVT);

QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_START_CCVT(PVOID pCCVT);

QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_STOP_CCVT(PVOID pCCVT);

QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_VIDEO_CCVT_CALLBACK( PVOID pCCVT /*IN*/, PF_VIDEO_CCVT_CALLBACK pCB /*IN*/, PVOID pUserData /*IN*/ );

QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_VIDEO_CCVT_UNCOMPRESSION_BUFFER( PVOID pCCVT /*IN*/, BYTE * pFrameBuffer /*IN*/, ULONG nFrameBufferLen /*IN*/);

QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_CCVT_CUSTOM_PROPERTY( PVOID pCCVT /*IN*/, ULONG nProperty, BYTE* pValue, ULONG nBytes);

QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_CCVT_CUSTOM_PROPERTY( PVOID pCCVT /*IN*/, ULONG nProperty, BYTE* pValue, ULONG nBytes);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif // __QCAP_CCVT_H__
