#ifndef __QCAP2_NVT_HDAL_H__
#define __QCAP2_NVT_HDAL_H__

#include "qcap2.h"

#define BOOL HD_BOOL
#include <hdal.h>
#undef BOOL

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// utilities
QRESULT qcap2_hd_mem_init_config(HD_COMMON_MEM_INIT_CONFIG* pMemInitConfig, int* pUserPoolIndex);
QRESULT qcap2_hd_init(HD_COMMON_MEM_INIT_CONFIG* pMemInitConfig);
QRESULT qcap2_hd_common_init();
QRESULT qcap2_hd_init1();
QRESULT qcap2_hd_common_sysconfig(ULONG nSysConfigType1, ULONG nSysConfigType2, ULONG nTypeMask1, ULONG nTypeMask2);
QRESULT qcap2_hd_mem_init(HD_COMMON_MEM_INIT_CONFIG* pMemInitConfig);
QRESULT qcap2_hd_module_init();

// qcap2_rcbuffer_t
QRESULT qcap2_rcbuffer_set_hd_video_frame(qcap2_rcbuffer_t* pRCBuffer, HD_VIDEO_FRAME* pVideoFrame);
QRESULT qcap2_rcbuffer_get_hd_video_frame(qcap2_rcbuffer_t* pRCBuffer, HD_VIDEO_FRAME** ppVideoFrame);
QRESULT qcap2_rcbuffer_alloc_hd_video_frame(qcap2_rcbuffer_t* pRCBuffer, HD_COMMON_MEM_POOL_TYPE nPoolType, HD_DIM oDim, HD_VIDEO_PXLFMT nPxlFmt, HD_COMMON_MEM_DDR_ID ddr);
QRESULT qcap2_rcbuffer_free_hd_video_frame(qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_rcbuffer_map_hd_video_frame(qcap2_rcbuffer_t* pRCBuffer, HD_COMMON_MEM_MEM_TYPE nMemType);
QRESULT qcap2_rcbuffer_unmap_hd_video_frame(qcap2_rcbuffer_t* pRCBuffer, bool bFlushCache);
QRESULT qcap2_rcbuffer_set_hd_audio_frame(qcap2_rcbuffer_t* pRCBuffer, HD_AUDIO_FRAME* pAudioFrame);
QRESULT qcap2_rcbuffer_get_hd_audio_frame(qcap2_rcbuffer_t* pRCBuffer, HD_AUDIO_FRAME** ppAudioFrame);
QRESULT qcap2_rcbuffer_set_hd_videoenc_bs(qcap2_rcbuffer_t* pRCBuffer, HD_VIDEOENC_BS* pVideoEncBs);
QRESULT qcap2_rcbuffer_get_hd_videoenc_bs(qcap2_rcbuffer_t* pRCBuffer, HD_VIDEOENC_BS** ppVideoEncBs);
QRESULT qcap2_rcbuffer_get_hd_mmap_addr(qcap2_rcbuffer_t* pRCBuffer, void** ppVideoMappedVirAddr);
QRESULT qcap2_rcbuffer_get_hd_videoenc_bufinfo(qcap2_rcbuffer_t* pRCBuffer, HD_VIDEOENC_BUFINFO* ppVideoEncBufInfo);

// qcap2_video_source_t
void qcap2_video_source_set_vcap_id(qcap2_video_source_t* pThis, int nVcapId);
void qcap2_video_source_set_vcap_id2(qcap2_video_source_t* pThis, int nVcapId);
void qcap2_video_source_set_hd_src_dim(qcap2_video_source_t* pThis, HD_DIM oSrcDim);
void qcap2_video_source_set_hd_src_pxl_fmt(qcap2_video_source_t* pThis, HD_VIDEO_PXLFMT nSrcPxlFmt);
void qcap2_video_source_set_vproc_id(qcap2_video_source_t* pThis, int nVprocId);
void qcap2_video_source_set_drv_config(qcap2_video_source_t* pThis, HD_VIDEOCAP_DRV_CONFIG* pDrvConfig);
void qcap2_video_source_set_ctrl_func(qcap2_video_source_t* pThis, HD_VIDEOCAP_CTRLFUNC nCtrlFunc);
void qcap2_video_source_set_vendor_isp_config(qcap2_video_source_t* pThis, const char* pszConfigUrl);

// qcap2_video_sink_t
void qcap2_video_sink_set_vout_id(qcap2_video_sink_t* pThis, int nVoutId);

// qcap2_audio_source_t
void qcap2_audio_source_set_acap_id(qcap2_audio_source_t* pThis, int nAcapId);

// qcap2_audio_sink_t
void qcap2_audio_sink_set_aout_id(qcap2_audio_sink_t* pThis, int nAoutId);
void qcap2_audio_sink_set_aout_output(qcap2_audio_sink_t* pThis, HD_AUDIOOUT_OUTPUT nAoutOutput);
QRESULT qcap2_audio_sink_set_aout_volume(qcap2_audio_sink_t* pThis, HD_AUDIOOUT_VOLUME nAoutVolume);

// qcap2_video_encoder_t
void qcap2_video_encoder_set_venc_id(qcap2_video_encoder_t* pThis, int nVencId);
void qcap2_video_encoder_set_in_id(qcap2_video_encoder_t* pThis, int nInId);
void qcap2_video_encoder_set_out_id(qcap2_video_encoder_t* pThis, int nOutId);
QRESULT qcap2_video_encoder_get_buf_info(qcap2_video_encoder_t* pThis, HD_VIDEOENC_BUFINFO* pVencBufInfo);
QRESULT qcap2_video_encoder_get_mmap_addr(qcap2_video_encoder_t* pThis, UINTPTR* pVencMappedVirAddr);

// qcap2_video_decoder_t
void qcap2_video_decoder_set_vdec_id(qcap2_video_decoder_t* pThis, int nVdecId);
void qcap2_video_decoder_set_in_id(qcap2_video_decoder_t* pThis, int nInId);
void qcap2_video_decoder_set_out_id(qcap2_video_decoder_t* pThis, int nOutId);
void qcap2_video_decoder_set_vproc_id(qcap2_video_decoder_t* pThis, int nVprocId);

// qcap2_audio_encoder_t
void qcap2_audio_encoder_set_aenc_id(qcap2_audio_encoder_t* pThis, int nAencId);

// qcap2_video_scaler_t
void qcap2_video_scaler_set_vproc_id(qcap2_video_scaler_t* pThis, int nVprocId);
void qcap2_video_scaler_set_out_id(qcap2_video_scaler_t* pThis, int nOutId);
void qcap2_video_scaler_set_hd_src_dim(qcap2_video_scaler_t* pThis, HD_DIM oSrcDim);
void qcap2_video_scaler_set_hd_src_pxl_fmt(qcap2_video_scaler_t* pThis, HD_VIDEO_PXLFMT nSrcPxlFmt);
void qcap2_video_scaler_set_hd_src_pipe(qcap2_video_scaler_t* pThis, HD_VIDEOPROC_PIPE nSrcPipe);
void qcap2_video_scaler_set_hd_src_isp_id(qcap2_video_scaler_t* pThis, int nSrcIspId);
void qcap2_video_scaler_set_ctrl_func(qcap2_video_scaler_t* pThis, HD_VIDEOPROC_CTRLFUNC nCtrlFunc);
void qcap2_video_scaler_set_ref_path_3dnr(qcap2_video_scaler_t* pThis, int nOutId);
void qcap2_video_scaler_set_out_crop(qcap2_video_scaler_t* pThis, HD_VIDEOPROC_CROP* oOutCrop);
void qcap2_video_scaler_set_out_depth(qcap2_video_scaler_t* pThis, int nOutDepth);
void qcap2_video_scaler_set_bind_enc(qcap2_video_scaler_t* pThis, int nVencId, int nVendInId);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_NVT_HDAL_H__