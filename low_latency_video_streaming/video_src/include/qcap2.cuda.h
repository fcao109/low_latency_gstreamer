#ifndef __QCAP2_CUDA_H__
#define __QCAP2_CUDA_H__

#include "qcap2.h"

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cudaEGL.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum qcap2_mask_comp_flags_t {
	QCAP2_MASK_COMP_NONE = 0x0000,
	QCAP2_MASK_COMP_INVERTED = 0x0001,
};

// utilities
QRESULT qcap2_cuda_device_synchronize();

// qcap2_av_frame_t
bool qcap2_av_frame_alloc_cuda_buffer(qcap2_av_frame_t* pFrame, int align, int valign); // cudaMalloc
void qcap2_av_frame_free_cuda_buffer(qcap2_av_frame_t* pFrame);
bool qcap2_av_frame_alloc_cuda_host_buffer(qcap2_av_frame_t* pFrame, unsigned int nFlags, int align, int valign); // cudaHostAlloc
void qcap2_av_frame_free_cuda_host_buffer(qcap2_av_frame_t* pFrame);
bool qcap2_av_frame_alloc_cuda_managed_buffer(qcap2_av_frame_t* pFrame, unsigned int nFlags, int align, int valign); // cudaMallocManaged
void qcap2_av_frame_free_cuda_managed_buffer(qcap2_av_frame_t* pFrame);

// qcap2_rcbuffer_t
QRESULT qcap2_rcbuffer_get_cures(qcap2_rcbuffer_t* pRCBuffer, CUgraphicsResource* pCUDAResource);
QRESULT qcap2_rcbuffer_get_mapped_eglframe(qcap2_rcbuffer_t* pRCBuffer, CUeglFrame* pEGLFrame);
QRESULT qcap2_rcbuffer_get_mapped_eglbuffer(qcap2_rcbuffer_t* pRCBuffer, uintptr_t pBuffer[3], int pPitch[3]);

// cuda kernels
QRESULT qcap2_kernel_sbs(qcap2_rcbuffer_t* pLeft, qcap2_rcbuffer_t* pRight, qcap2_rcbuffer_t* pDst);
QRESULT qcap2_kernel_mask_comp(qcap2_rcbuffer_t* pBkg, qcap2_rcbuffer_t* pMask, int nYCbCr[3], int nFlags, qcap2_rcbuffer_t* pDst);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_CUDA_H__