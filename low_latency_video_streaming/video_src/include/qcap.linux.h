#ifndef __QCAP_LINUX_H__
#define __QCAP_LINUX_H__

#include "qcap.common.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#pragma pack(push)
#pragma pack(1)

struct qcap_av_frame_t {
	BYTE* pData[8]; // data pointer for each image plane
	int nPitch[8]; // pitch for each image plane

	PVOID pPrivateData0;

	// width & height of the video frame
	int nWidth;
	int nHeight;

	int nSamples; // number of audio samples (per channel)

	int nFormat; // ref to FFmpeg
};

struct qcap_av_packet_t {
	PVOID pPrivateData0;

	LONGLONG nPTS; // in microseconds
	LONGLONG nDTS; // in microseconds
	BYTE* pData;
	int nSize;
	int nIndex;
	int nFlags;
};

#pragma pack(pop)

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif // __QCAP_LINUX_H__