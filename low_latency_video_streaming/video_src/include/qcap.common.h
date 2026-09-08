#ifndef __QCAP_COMMON_H__
#define __QCAP_COMMON_H__

#include "qcap.h"
#include "qcap.windef.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum {
	QCAP_RS_ERROR_END_OF_FILE = 0x81000000,
};

// RECORD FLAG EXTEND
//
enum {
	QCAP_RECORD_FLAG_PASS_THROUGH					= 0x80000000,
	QCAP_RECORD_FLAG_MODIFY_ONLY					= 0x40000000,
	QCAP_RECORD_FLAG_ROTATE_FIT						= 0x20000000,
	QCAP_RECORD_FLAG_ROTATE_STRETCH					= 0x10000000,
};

// COLOR SPACE TYPE
enum {
	QCAP_COLORSPACE_TYPE_BGRP = 0x50524742,
	QCAP_COLORSPACE_TYPE_GBRP = 0x50524247,
	// QCAP_COLORSPACE_TYPE_YV21 = QCAP_COLORSPACE_TYPE_I420,
	QCAP_COLORSPACE_TYPE_RGBP_FLOAT = 0x50464752,
	QCAP_COLORSPACE_TYPE_Y444 = 0x34343459,
	QCAP_COLORSPACE_TYPE_YUVA420P = 0x32314159,
	QCAP_COLORSPACE_TYPE_NV16 = 0x3631564E,
	QCAP_COLORSPACE_TYPE_NV24 = 0x3432564E,
	QCAP_COLORSPACE_TYPE_Y8 = 0x20203859,
	QCAP_COLORSPACE_TYPE_Y210 = 0x30313259,
	QCAP_COLORSPACE_TYPE_Y410 = 0x30313459,
	QCAP_COLORSPACE_TYPE_YV16 = 0x36315659,
	QCAP_COLORSPACE_TYPE_I422 = 0x32323449,
	QCAP_COLORSPACE_TYPE_XV20 = 0x30325658,
	QCAP_COLORSPACE_TYPE_XV15 = 0x35315658,
	QCAP_COLORSPACE_TYPE_RGBA = 0x00000020,

	QCAP_COLORSPACE_TYPE_RGB444LE = 0xFF000001,
	QCAP_COLORSPACE_TYPE_RGB444BE = 0xFF000002,
	QCAP_COLORSPACE_TYPE_BGR444LE = 0xFF000003,
	QCAP_COLORSPACE_TYPE_BGR444BE = 0xFF000004,

	QCAP_COLORSPACE_TYPE_UNDEFINED = 0xFFFFFFFF,
};

// INPUT VIDEO SOURCE
//
enum {
	QCAP_INPUT_TYPE_TVI = 100,
	QCAP_INPUT_TYPE_SENSOR,
	QCAP_INPUT_TYPE_UNDEFINED = 0xFFFFFFFF,
	QCAP_INPUT_TYPE_DISPLAY_PORT_MST = 0x80000002,
	QCAP_INPUT_TYPE_DISPLAY_PORT_SST = 0x80000102,
};

// INPUT AUDIO SOURCE
//
enum {
	QCAP_INPUT_TYPE_MICROPHONE = 1002,
	QCAP_INPUT_TYPE_AUDIO_MIXER,
};

// ENCODER TYPE (RECORD PROPERTY)
//
enum {
	QCAP_ENCODER_TYPE_ZZNVCODEC = 1005,
	QCAP_ENCODER_TYPE_GST_SOFTWARE,
	QCAP_ENCODER_TYPE_GST_VAAPI,
	QCAP_ENCODER_TYPE_GST_NVENC,
	QCAP_ENCODER_TYPE_GST_XLNK,
	QCAP_ENCODER_TYPE_CCIMS,
	QCAP_ENCODER_TYPE_NDICODEC,
	QCAP_ENCODER_TYPE_HIMPP,
	QCAP_ENCODER_TYPE_RKMPP,
	QCAP_ENCODER_TYPE_ALLEGRO,
	QCAP_ENCODER_TYPE_VAAPI,
	QCAP_ENCODER_TYPE_EXTERNAL,
	QCAP_ENCODER_TYPE_V4L2SRC,
	QCAP_ENCODER_TYPE_V4L2NVCODEC,
	QCAP_ENCODER_TYPE_NVT_HDAL,
	QCAP_ENCODER_TYPE_ZZNVCODEC2, // built from qcap2
	QCAP_ENCODER_TYPE_ALLEGRO2, // v2023.2
	QCAP_ENCODER_TYPE_NVMMAPI,
	QCAP_ENCODER_TYPE_UNDEFINED = 0xFFFFFFFF,
};

// DECODER TYPE (PLAYBACK PROPERTY)
//
enum {
	QCAP_DECODER_TYPE_ZZNVCODEC		= 1005,
	QCAP_DECODER_TYPE_GST_SOFTWARE,
	QCAP_DECODER_TYPE_GST_VAAPI,
	QCAP_DECODER_TYPE_GST_NVDEC,
	QCAP_DECODER_TYPE_GST_XLNK,
	QCAP_DECODER_TYPE_CCIMS,
	QCAP_DECODER_TYPE_NDICODEC,
	QCAP_DECODER_TYPE_HIMPP,
	QCAP_DECODER_TYPE_RKMPP,
	QCAP_DECODER_TYPE_ALLEGRO,
	QCAP_DECODER_TYPE_NVT_HDAL,
	QCAP_DECODER_TYPE_UNDEFINED = 0xFFFFFFFF,
};

// AUDIO SAMPLE FORMAT
//
enum {
	QCAP_SAMPLE_FMT_NONE		= -1,
	QCAP_SAMPLE_FMT_U8			= 0,
	QCAP_SAMPLE_FMT_S16,
	QCAP_SAMPLE_FMT_S32,
	QCAP_SAMPLE_FMT_FLT,
	QCAP_SAMPLE_FMT_DBL,
	QCAP_SAMPLE_FMT_U8P,
	QCAP_SAMPLE_FMT_S16P,
	QCAP_SAMPLE_FMT_S32P,
	QCAP_SAMPLE_FMT_FLTP,
	QCAP_SAMPLE_FMT_DBLP,
};

// CODEC FORMAT (RECORD PROPERTY)
//
enum {
	QCAP_ENCODER_FORMAT_PCM_LE		= QCAP_ENCODER_FORMAT_PCM,
	QCAP_ENCODER_FORMAT_MJPEG		= 1000,
	QCAP_ENCODER_FORMAT_JPEG,
	QCAP_ENCODER_FORMAT_PCM_BE,
	QCAP_ENCODER_FORMAT_S302M,
	QCAP_ENCODER_FORMAT_SHQ0,
	QCAP_ENCODER_FORMAT_SHQ2,
	QCAP_ENCODER_FORMAT_PCM_FLT,
	QCAP_ENCODER_FORMAT_UNDEFINED	= 0xFFFFFFFF,

	QCAP_DECODER_FORMAT_AAC 		= QCAP_ENCODER_FORMAT_AAC,
	QCAP_DECODER_FORMAT_AAC_ADTS	= QCAP_ENCODER_FORMAT_AAC_ADTS,
	QCAP_DECODER_FORMAT_PCM			= QCAP_ENCODER_FORMAT_PCM,
	QCAP_DECODER_FORMAT_PCM_LE		= QCAP_ENCODER_FORMAT_PCM_LE,
	QCAP_DECODER_FORMAT_MJPEG		= QCAP_ENCODER_FORMAT_MJPEG,
	QCAP_DECODER_FORMAT_JPEG		= QCAP_ENCODER_FORMAT_JPEG,
	QCAP_DECODER_FORMAT_PCM_BE		= QCAP_ENCODER_FORMAT_PCM_BE,
	QCAP_DECODER_FORMAT_S302M		= QCAP_ENCODER_FORMAT_S302M,
	QCAP_DECODER_FORMAT_SHQ0		= QCAP_ENCODER_FORMAT_SHQ0,
	QCAP_DECODER_FORMAT_SHQ2		= QCAP_ENCODER_FORMAT_SHQ2,
	QCAP_DECODER_FORMAT_PCM_FLT		= QCAP_ENCODER_FORMAT_PCM_FLT,
	QCAP_DECODER_FORMAT_UNDEFINED	= 0xFFFFFFFF,
};

// RECORD MODE (RECORD PROPERTY)
//
enum {
	QCAP_RECORD_MODE_FIXQP	= 0x1000,
};

// constant
enum {
	QCAP_HWND_DRM_PLANE_ID_MASK = 0xFFFF0000,
};

// global custom property
enum {
	// for linux_base
	QCAP_PROPERTY_DISPLAY_SYSTEM = 10000,			// in,out: ULONG
	QCAP_PROPERTY_GRAPHIC_WINDOW_SYSTEM,			// in,out: ULONG
	QCAP_PROPERTY_BUTTON_EVENT,						// out: QCAP_PROPERTY_BUTTON_EVENT_RESPONSE
	QCAP_PROPERTY_UB3300_DEVINFO,					// in, ULONG[3]: [HIDRAW, VIDEO4LINUX, ALSA]

	// for hisiv_base
	QCAP_PROPERTY_FILE_PIPELINE_MODE = 20000,		// in,out: ULONG
	QCAP_PROPERTY_TSENSOR_DETECT,					// in,out: ULONG
	QCAP_PROPERTY_HDMI_VO_PARAM,
	QCAP_PROPERTY_COLOR_RANGE,
	QCAP_PROPERTY_VO_PARAM,
	QCAP_PROPERTY_VO_CSC_PARAM,
	QCAP_PROPERTY_MCU_VERSION,
	QCAP_PROPERTY_COPY_PROTECT_KEY,
	QCAP_PROPERTY_COPY_PROTECT_CHECK,
	QCAP_PROPERTY_GPIO_OUTPUT,
	QCAP_PROPERTY_AUDIO_MIXER_SOURCE_VOLUME,
	QCAP_PROPERTY_LED,								// in: QCAP_PROPERTY_LED_VALUE
	QCAP_PROPERTY_BUTTON,							// out: QCAP_PROPERTY_BUTTON_RESPONSE
	QCAP_PROPERTY_VOLUME,							// in: QCAP_PROPERTY_VOLUME_VALUE
	QCAP_PROPERTY_NDI_MODE,							// out: QCAP_PROPERTY_NDI_MODE_RESPONSE
	QCAP_PROPERTY_NOISE_GATE,						// in: QCAP_PROPERTY_NOISE_GATE_VALUE
	QCAP_PROPERTY_NDI_KEY,							// in/out: QCAP_PROPERTY_NDI_KEY_VALUE
	QCAP_PROPERTY_BUTTON_EX,						// out: QCAP_PROPERTY_BUTTON_EX_RESPONSE
	QCAP_PROPERTY_MENU,								// in: QCAP_PROPERTY_MENU_VALUE

	// for both use
	QCAP_PROPERTY_LED_EX = 30000,					// in: QCAP_PROPERTY_LED_EX_VALUE
	QCAP_PROPERTY_BUTTON_FD,						// out: int
	QCAP_PROPERTY_ACTIVATE,							// in: ULONG
	QCAP_PROPERTY_VERSION,							// in: int
	QCAP_PROPERTY_VO_BACKEND,						// [in/out, ULONG] 0:sdl, 1:gstreamer, 2:davmf, 3:vitis
};

// device custom property
enum {
	// compatible with QCAP-WIN
	QCAP_DEVPROP_FIRMWARE_VERSION = 6,				// [out, ULONG]
	QCAP_DEVPROP_TRANSFER_MODE = 10,				// [in/out, ULONG] 0:bulk, 1:isoc
	QCAP_DEVPROP_HARDWARE_VERSION = 15,				// [out, ULONG]
	QCAP_DEVPROP_DEVICE_ALIAS = 20,					// [out, char*] C-string (null terminated)
	QCAP_DEVPROP_SERIAL_NUMBER = 27,				// [in, char [17]] 16 chars C-string (null terminated)
	QCAP_DEVPROP_HARDWARE_DEINTERLACE = 200,		// [in, ULONG] 0:enabled, 1:disabled
	QCAP_DEVPROP_720_OUTPUT = 214,					// [in, ULONG] 0:960, 1:720
	QCAP_DEVPROP_CAMERA_TYPE = 226,					// [in, ULONG] 0:TVI, 1:AHD, 2:CVI
	QCAP_DEVPROP_OUTPUT_COLORRANGE = 231,			// [in, ULONG] 0:bypass, 1:expand, 2:shrink
	QCAP_DEVPROP_AUDIO_VOLUME = 251,				// [in/out, ULONG]
	QCAP_DEVPROP_AUDIO_LINE_IN_MODE = 252,			// [in, ULONG] 0:phonejack, 1:pin
	QCAP_DEVPROP_AUDIO_INPUT = 255,					// [in, ULONG] 0:HDMI, 1:Line-in, 2:Mic-in, 3:HDMI+Line-in, 4:HDMI+Mic-in
	QCAP_DEVPROP_AUDIO_CHANNEL = 257,				// [in, ULONG] 0:normal, 8:8ch
	QCAP_DEVPROP_MEDICAL_MODE = 276,				// [in, ULONG] 0:normal, 1:medical
	QCAP_DEVPROP_MUTE_LOOPTHROUGH = 344,			// [in/out, ULONG] 0:unmute, 1:mute
	QCAP_DEVPROP_COLORSPACE = 370,					// [out, ULONG] 2:REC709, 1: SMPTE170M,  3:BT2020
	QCAP_DEVPROP_BITRATE = 403,						// [in, ULONG]
	QCAP_DEVPROP_SURROUND_MODE = 610,				// [in/out, ULONG] 0:normal, 2:two-span-mode, 3:three-span-mode 4:quad-mode
	QCAP_DEVPROP_SDI12G_MODE = 613,					// [in, ULONG] 0:2SI, 1:SQD
	QCAP_DEVPROP_SIGNAL_MODE = 613,					// [in, ULONG] 0:MST, 1:SST
	QCAP_DEVPROP_FIRMWARE_PATH = 800,				// [in, char*]
	QCAP_DEVPROP_FIRMWARE_UPDATE = 801,				// [in, ULONG] 1:start update, 2:recovery mode update FW
	QCAP_DEVPROP_OSD_X = 915,						// [in, LONG]
	QCAP_DEVPROP_OSD_Y = 916,						// [in, LONG]
	QCAP_DEVPROP_OSD_FONT_SIZE = 917,				// [in, LONG]
	QCAP_DEVPROP_OSD_NUMBER = 920,					// [in, LONG]
	QCAP_DEVPROP_OSD_TEXT = 921,					// [in, char*, NULL-terminated]
	QCAP_DEVPROP_OSD_FONT_STYLE = 929,				// [in, LONG]
	QCAP_DEVPROP_BOARD_MEMORY = 958,				// [in/out: BYTE*, <= 16 bytes]
	QCAP_DEVPROP_BOARD_MEMORY_ACCESS = 959,			// [in, LONG]
	QCAP_DEVPROP_VBI_LINES_ENABLE = 236,			// [in, ULONG] 0:disable, 1:enable
	QCAP_DEVPROP_VBI_LINES = 237,					// [in, ULONG] 0~84
	QCAP_DEVPROP_VBI_MIX_MODE = 229,				// [in, ULONG] 0:VANC, 1:HANC
	QCAP_DEVPROP_VBI_OUTPUT_VIDEO_TYPE = 238,		// [in, ULONG] 0:frame mode, 1:field mode
	QCAP_DEVPROP_UPDATE_EDID = 965,					// PCIE capture card ==> [in, ULONG] 0:end, 1:begin
	QCAP_DEVPROP_UVC_EDID_DATA = 965,				// UVC capture card ==> [in/out, char[256]]
	QCAP_DEVPROP_EDID_OFFSET = 966,					// [in, ULONG] byte offset of EDID data
	QCAP_DEVPROP_EDID_DATA = 967,					// [in, ULONG] byte data of EDID data corresponds to offset
	QCAP_DEVPROP_READ_EDID_DATA = 967,				// [in, ULONG] byte data of EDID data corresponds to offset
	QCAP_DEVPROP_EDID_ACCESS = 968,					// [in, ULONG]
	QCAP_DEVPROP_EDID_TYPE = 978,					// [in, ULONG] 1:HDMI, 2:VGA, 3:DVI, 4:DP
	QCAP_DEVPROP_VIDEO_DEVICE_NAME = 1201,			// [in/out, char []] C-string (null terminated)
	QCAP_DEVPROP_AUDIO_DEVICE_NAME = 1202,			// [in/out, char []] C-string (null terminated)
	QCAP_DEVPROP_FLASH_DATA = 1200,					// [in, ULONG] 1:write data to flash rom
	QCAP_DEVPROP_VIDEO_MULTICH_MASK = 269,			// [in, ULONG] bitmask for channel selection
	QCAP_DEVPROP_VIDEO_MULTICH_SUPPORT = 397,		// [in, ULONG] 0:single, 1:multi
	QCAP_DEVPROP_VIDEO_VGA_OFFSET_X = 221,			// [in, LONG]
	QCAP_DEVPROP_VIDEO_VGA_OFFSET_Y = 222,			// [in, LONG]
	QCAP_DEVPROP_ANALOG_PTZ_COMMAND = 810,			// [in, LONG]

	// for linux_base
	QCAP_DEVPROP_TIME_ZONE = 10000,					// [in, LONG]
	QCAP_DEVPROP_NO_SIGNAL_NO_OUTPUT,				// [in, ULONG] 0:no-signal image, 1:freeze
	QCAP_DEVPROP_NO_SIGNAL_YUV_PATTERN,				// [in, ULONG] 0x00YYVVUU
	QCAP_DEVPROP_DOUBLE_FRAME_RATE,					// [in/out, ULONG] boolean value
	QCAP_DEVPROP_IO_METHOD,							// [in/out, ULONG] 0:mmap, 1:userptr
	QCAP_DEVPROP_VO_BACKEND,						// [in/out, ULONG] 0:sdl, 1:gstreamer, 2:davmf, 3:vitis
	QCAP_DEVPROP_PSF,								// [out, ULONG] 0:progressive, 1:PSF
	QCAP_DEVPROP_AUDIO_PERIOD_SIZE,					// [in, ULONG] ALSA period size
	QCAP_DEVPROP_SONY_CAMERA_RESET,					// [in, ULONG] 0: disable, 1:enable
	QCAP_DEVPROP_SWITCH_PCIE_SLOT,					// [in, ULONG] 6T0 0:PCIE(HDMI), 1:M2(SDI)
	QCAP_DEVPROP_DRM_POS_X,							// [in/out: ULONG] boolean value
	QCAP_DEVPROP_DRM_POS_Y,							// [in/out: ULONG] boolean value
	QCAP_DEVPROP_DRM_WIDTH,							// [in/out: ULONG] boolean value
	QCAP_DEVPROP_DRM_HEIGHT,						// [in/out: ULONG] boolean value
	QCAP_DEVPROP_CV0830_DEVINFO,					// in, ULONG[2]: [VIDEO4LINUX, ALSA]
	QCAP_DEVPROP_ANALOG_VIDEO_WIDTH,				// [in, ULONG] 0:960, other:720
	QCAP_DEVPROP_VIDEO_SOURCE_FORMAT,				// [out, ULONG] 0:YUV422, 1:YUV444, 2:RGB
	QCAP_DEVPROP_RESET_570_DEVICE,					// [in, ULONG] 1: reset
	QCAP_DEVPROP_DMA_ERROR,							// [out, ULONG]
	QCAP_DEVPROP_FORCE_WIDTH,						// [in, ULONG]
	QCAP_DEVPROP_FORCE_HEIGHT,						// [in, ULONG]
	QCAP_DEVPROP_FORCE_USERPTR,						// [in, ULONG]
	QCAP_DEVPROP_INTERLACED_FIELD_MODE,				// [in, ULONG]
	QCAP_DEVPROP_SURROUND_TYPE,						// [in, ULONG]  //0:Left-right(default), 1:Top-down
	QCAP_DEVPROP_QUANTIZATION,						// [out, ULONG] 1:FULL, 2:Limited
	QCAP_DEVPROP_INPUT_TYPE_MCU_AUTO,				// [in, ULONG] //1:True
	QCAP_DEVPROP_NVT_HDAL_VPRC_ID,					// [in, ULONG] 0xFFAABBCC AA=Id BB=In CC=Out
	QCAP_DEVPROP_NVT_HDAL_VOUT_WIDTH,				// [in, ULONG] video output width
	QCAP_DEVPROP_NVT_HDAL_VOUT_HEIGHT,				// [in, ULONG] video output height
	QCAP_DEVPROP_NVT_HDAL_VOUT_FRAME_RATE,			// [in, DOUBLE] video output height
	QCAP_DEVPROP_Y210_REDUCED_MODE,					// [in, ULONG] 0:disable, 1:enable

	// for hisiv_base
	QCAP_DEVPROP_LINEIN_SOURCE = 20000,				// in,out: ULONG
	QCAP_DEVPROP_CLONE_WINDOW,						// in,out: HWND
	QCAP_DEVPROP_ROTATE,							// in,out: ULONG (in degrees, 0~360)
	QCAP_DEVPROP_COMPOSITE_SOURCE,					// in,out: ULONG
	QCAP_DEVPROP_PIPELINE_MODE,						// in,out: ULONG
	QCAP_DEVPROP_LOWDELAY_MODE,						// in,out: ULONG
	QCAP_DEVPROP_BIND_BS,							// in,out: PVOID (broadcast server)
	QCAP_DEVPROP_BIND_BS_0 = QCAP_DEVPROP_BIND_BS,	// in,out: PVOID (broadcast server)
	QCAP_DEVPROP_BIND_BS_1,							// in,out: PVOID (broadcast server)
	QCAP_DEVPROP_BIND_BS_2,							// in,out: PVOID (broadcast server)
	QCAP_DEVPROP_BIND_BS_3,							// in,out: PVOID (broadcast server)
	QCAP_DEVPROP_BIND_SR,							// in,out: UINT (share record number)
	QCAP_DEVPROP_BIND_SR_0 = QCAP_DEVPROP_BIND_SR,	// in,out: UINT (share record number)
	QCAP_DEVPROP_BIND_SR_1,							// in,out: UINT (share record number)
	QCAP_DEVPROP_BIND_SR_2,							// in,out: UINT (share record number)
	QCAP_DEVPROP_BIND_SR_3,							// in,out: UINT (share record number)
	QCAP_DEVPROP_AI_TRACK_MODE,						// in,out: ULONG (0:normal, 1:both_left, 2:both_right)
	QCAP_DEVPROP_CROP_RATIO,						// in: [x:LONG, y:LONG, w:LONG, h:LONG] (0~999)
	QCAP_DEVPROP_BIND_WND_0,						// in,out: HWND
	QCAP_DEVPROP_BIND_WND_1,						// in,out: HWND
	QCAP_DEVPROP_BIND_WND_2,						// in,out: HWND
	QCAP_DEVPROP_BIND_WND_3,						// in,out: HWND
	QCAP_DEVPROP_AUDIO_RENDERER_VOLUME,				// in,out: ULONG
	QCAP_DEVPROP_ISP_PUB,							// in,out: ISP_PUB_ATTR_S
	QCAP_DEVPROP_ISP_STATISTICS_CFG,				// in,out: ISP_STATISTICS_CFG_S
	QCAP_DEVPROP_ISP_VDTIMEOUT,						// out: ULONG[2] { ISP_VD_TYPE_E, HI_U32 }
	QCAP_DEVPROP_ISP_FOCUS_STATISTICS,				// out: ISP_AF_STATISTICS_S
	QCAP_DEVPROP_ISP_EXPOSURE,						// in,out: ISP_EXPOSURE_ATTR_S
	QCAP_DEVPROP_ISP_WB,							// in,out: ISP_WB_ATTR_S
	QCAP_DEVPROP_ISP_LIGHTBOXGAIN,					// out: ISP_AWB_Calibration_Gain_S
	QCAP_DEVPROP_ISP_EXPOSUREINFO,					// out: ISP_EXP_INFO_S
	QCAP_DEVPROP_VPSS_NR_ATTR,						// in: [HI_BOOL, VPSS_NR_ATTR_S]
	QCAP_DEVPROP_ISP_CSC,							// in,out: ISP_CSC_ATTR_S
	QCAP_DEVPROP_ISP_SATURATION,					// in: ISP_SATURATION_ATTR_S
	QCAP_DEVPROP_VI_LDC,							// in,out: VI_LDC_ATTR_S
	QCAP_DEVPROP_ISP_AWB_EX,						// in,out: ISP_AWB_ATTR_EX_S
	QCAP_DEVPROP_ISP_WB_INFO,						// out,ISP_WB_INFO_S
	QCAP_DEVPROP_VPSS_MIRROR_ATTR,					// in:[HI_BOOL]
	QCAP_DEVPROP_VPSS_FLIP_ATTR,					// in:[HI_BOOL]
	QCAP_DEVPROP_VPSS_LDC_ATTR,						// in: [HI_BOOL, LDC_ATTR_S]
	QCAP_DEVPROP_ISP_DRC_ATTR,						// in,out:ISP_DRC_ATTR_S
	QCAP_DEVPROP_ISP_LDCI_ATTR, 					// in,out:ISP_LDCI_ATTR_S
	QCAP_DEVPROP_ISP_DEHAZE_ATTR, 					// in,out:ISP_DEHAZE_ATTR_S
	QCAP_DEVPROP_ISP_GAMMA_ATTR, 					// in,out:ISP_GAMMA_ATTR_S
	QCAP_DEVPROP_ISP_COLORMATRIX_ATTR, 				// in,out:ISP_COLORMATRIX_ATTR_S
	QCAP_DEVPROP_ISP_NR_ATTR, 						// in,out:ISP_NR_ATTR_S
	QCAP_DEVPROP_ISP_SHARPEN_ATTR,					// in,out:ISP_SHARPEN_ATTR_S
	QCAP_DEVPROP_ISP_SHADING_ATTR,					// in,out:ISP_SHADING_ATTR_S
	QCAP_DEVPROP_ISP_SHADING_LUT_ATTR,				// in,out:ISP_SHADING_LUT_ATTR_S
	QCAP_DEVPROP_ISP_DEMOSAIC_ATTR,					// in,out:ISP_DEMOSAIC_ATTR_S
	QCAP_DEVPROP_ISP_DP_DYNAMIC_ATTR,				// in,out:ISP_DP_DYNAMIC_ATTR_S
	QCAP_DEVPROP_VPSS_NRX_PARAM,					// in: [VPSS_GRP_NRX_PARAM_S]
	QCAP_DEVPROP_VPSS_CHN_ATTR_0,					// in,out: VPSS_CHN_ATTR_S
	QCAP_DEVPROP_VPSS_CHN_ATTR_1,					// in,out: VPSS_CHN_ATTR_S
	QCAP_DEVPROP_VPSS_CHN_ATTR_2,					// in,out: VPSS_CHN_ATTR_S
	QCAP_DEVPROP_VPSS_CHN_ATTR_3,					// in,out: VPSS_CHN_ATTR_S
	QCAP_DEVPROP_VPSS_CHN_CROP_0,					// in,out: VPSS_CROP_INFO_S
	QCAP_DEVPROP_VPSS_CHN_CROP_1,					// in,out: VPSS_CROP_INFO_S
	QCAP_DEVPROP_VPSS_CHN_CROP_2,					// in,out: VPSS_CROP_INFO_S
	QCAP_DEVPROP_VPSS_CHN_CROP_3,					// in,out: VPSS_CROP_INFO_S

	// for both use
	QCAP_DEVPROP_IDEAL_PTS = 30000,					// [in/out, ULONG] boolean value
	QCAP_DEVPROP_SRCDET_ONLY,						// [in/out, ULONG] boolean value
	QCAP_DEVPROP_ERROR_MODE,						// [out, ULONG] current error mode
	QCAP_DEVPROP_GENLOCK_SOURCE,					// [in, ULONG] //0:free run, 1:GS12170, 2:Ext_Vsync

	// for private use
	QCAP_DEVPROP_VIDEO_FRAME_OFFSET = 40000,		// [in, ULONG] buffer index offset
};

// Compatibility spelling used by legacy QCAP VBI samples.
#define QCAP_PROPERTY_VBI_OUTPUT_VIDEO_TYPE QCAP_DEVPROP_VBI_OUTPUT_VIDEO_TYPE

// share record custom property
enum {
	// QCAP_SHARE_RECORD_PROPERTY_SOURCE_FRAME_RATE = 100, // in: double, DEPRECATED!

	QCAP_SRPROP_SOURCE_FRAME_RATE = 100,	// in: double
	QCAP_SRPROP_ROTATE,						// in,out: ULONG (in degrees, 0~360)
	QCAP_SRPROP_VO_BACKEND,					// [in/out, ULONG] 0:sdl, 1:gstreamer, 2:davmf, 3:vitis
	QCAP_SRPROP_VENC_OSD,					// [in/out, ULONG] boolean value
	QCAP_SRPROP_ENABLE_FF,					// [in/out, ULONG] boolean value
	QCAP_SRPROP_VENC_LOW_DELAY,				// [in/out, ULONG] boolean value
	QCAP_SRPROP_VENC_RTMP_COMPAT,			// [in/out, ULONG] boolean value
	QCAP_SRPROP_VDEC_LOW_LATENCY,		 	// [in/out: ULONG] boolean value
	QCAP_SRPROP_VDEC_REWIND,			 	// [in/out: ULONG] boolean value
	QCAP_SRPROP_VENC_TIME_SCALE_FACTOR,		// [in/out, ULONG] time scale factor
	QCAP_SRPROP_DRM_POS_X,					// [in/out: ULONG] boolean value
	QCAP_SRPROP_DRM_POS_Y,					// [in/out: ULONG] boolean value
	QCAP_SRPROP_DRM_WIDTH,					// [in/out: ULONG] boolean value
	QCAP_SRPROP_DRM_HEIGHT,					// [in/out: ULONG] boolean value
	QCAP_SRPROP_FB_RESET,					// [in/out: ULONG] boolean value
	QCAP_SRPROP_VENC_HIGH_PERF,				// [in/out, ULONG] boolean value
	QCAP_SRPROP_FMP4_LOW_LATENCY,			// [in/out, ULONG] boolean value
	QCAP_SRPROP_NVT_HDAL_VENC_ID,			// [in, ULONG] 0xFFAABBCC AA=Id BB=In CC=Out
	QCAP_SRPROP_NVT_HDAL_VDEC_ID,			// [in, ULONG] 0xFFAABBCC AA=Id BB=In CC=Out
	QCAP_SRPROP_NVT_HDAL_VPRC_ID,			// [in, ULONG] 0xFFAABBCC AA=Id BB=In CC=Out
	QCAP_SRPROP_NVT_HDAL_VDEC_VPRC_ID,		// [in, ULONG] 0xFFAABBCC AA=Id BB=In CC=Out
	QCAP_SRPROP_NVT_HDAL_VOUT_WIDTH,		// [in, ULONG] video output width
	QCAP_SRPROP_NVT_HDAL_VOUT_HEIGHT,		// [in, ULONG] video output height
	QCAP_SRPROP_NVT_HDAL_VOUT_FRAME_RATE,	// [in, DOUBLE] video output height
	QCAP_SRPROP_NVT_HDAL_VENC_NATIVE_BUFFER,// [in, ULONG] video encoder native buffer boolean value
	QCAP_SRPROP_COLOR_RANGE,				// [in, ULONG] QCAP_COLORRANGE_TYPE_xxx

	// private use only
	QCAP_SRPROP_BINDING_VPSS = 10800,		// in: [ULONG, ULONG]
	QCAP_SRPROP_BINDING_AVS,				// in: [ULONG, ULONG]
};

// broadcast server custom property
enum {
	// QCAP_PROPERTY_NDI_CONFIG				= 100, // in,out: char*, DEPRECATED!

	QCAP_BSPROP_NDI_CONFIG = 100,			// in,out: char*
	QCAP_BSPROP_SENDING_IP,					// in,out: char*
	QCAP_BSPROP_SOURCE_FRAME_RATE,			// in: double
	QCAP_BSPROP_RECEIVING_IP,				// in,out: char*
	QCAP_BSPROP_MULTICAST_IP,				// in,out: char*
	QCAP_BSPROP_FRAME_RATE,					// in: [double, double]
	QCAP_BSPROP_NDI_TALLY_GET,				// out: QCAP_BPROP_NDI_TALLY_VALUE
	QCAP_BSPROP_NDI_TALLY_SET,				// in: QCAP_BPROP_NDI_TALLY_VALUE
	QCAP_BSPROP_NDI_TALLY_TIMEOUT,			// in/out: ULONG
	QCAP_BSPROP_ENABLE_FF,					// [in/out, ULONG] boolean value
	QCAP_BSPROP_ENABLE_LOW_DELAY,			// [in/out, ULONG] boolean value
	QCAP_BSPROP_ENABLE_RTMP_COMPAT,			// [in/out, ULONG] boolean value
	QCAP_BSPROP_ERROR_MODE,					// [out, LONG] error mode
	QCAP_BSPROP_TIME_SCALE_FACTOR,			// [in/out, ULONG] time scale factor
	QCAP_BSPROP_ENABLE_HIGH_PERF,			// [in/out, ULONG] boolean value
	QCAP_BSPROP_BACKCHANNEL,				// [in/out, QCAP_BPROP_BACKCHANNEL_VALUE]
	QCAP_BSPROP_SDP,						// out: char*
	QCAP_BSPROP_NVT_HDAL_VENC_ID,			// [in, ULONG] ULONG 0xFFAABBCC AA=Id BB=In CC=Out
	QCAP_BSPROP_NVT_HDAL_VPRC_ID,			// [in, ULONG] ULONG 0xFFAABBCC AA=Id BB=In CC=Out
	QCAP_BSPROP_HLS_ID3,					// [in, ULONG] boolean value
	QCAP_BSPROP_COLOR_RANGE,				// [in, ULONG] QCAP_COLORRANGE_TYPE_xxx

	// private use only
	QCAP_BSPROP_BINDING_VPSS = 10800,		// in: [ULONG, ULONG]
	QCAP_BSPROP_BINDING_AVS,				// in: [ULONG, ULONG]
};

// broadcast client custom property
enum {
	QCAP_BCPROP_SENDING_IP = 100,				// in,out: char*
	QCAP_BCPROP_PIPELINE_MODE,					// in,out: ULONG
	QCAP_BCPROP_VIDEO_DECODE,					// in,out: BOOL
	QCAP_BCPROP_AUDIO_DECODE,					// in,out: BOOL
	QCAP_BCPROP_NDI_PTZ_IS_SUPPORTED,			// out: BOOL
	QCAP_BCPROP_NDI_PTZ_ZOOM,			 		// in: float 0.0 (zoomed in) ... 1.0 (zoomed out)
	QCAP_BCPROP_NDI_PTZ_ZOOM_SPEED,			 	// in: float -1.0 (zoom outwards) ... +1.0 (zoom inwards)
	QCAP_BCPROP_NDI_PTZ_PAN_TILT,			 	// in: [float -1.0 (left) ... 0.0 (centered) ... +1.0 (right), float -1.0 (bottom) ... 0.0 (centered) ... +1.0 (top)]
	QCAP_BCPROP_NDI_PTZ_PAN_TILT_SPEED,			// in: [float -1.0 (moving right) ... 0.0 (stopped) ... +1.0 (moving left), float -1.0 (down) ... 0.0 (stopped) ... +1.0 (moving up)]
	QCAP_BCPROP_NDI_PTZ_STORE_PRESET,			// in: int 0 ... 99
	QCAP_BCPROP_NDI_PTZ_RECALL_PRESET,			// in: [int 0 ... 99, float 0.0(as slow as possible) ... 1.0(as fast as possible) The speed at which to move to the new preset]
	QCAP_BCPROP_NDI_PTZ_AUTO_FOCUS,				// in: NULL
	QCAP_BCPROP_NDI_PTZ_FOCUS,					// in: float 0.0 (focused to infinity) ... 1.0 (focused as close as possible)
	QCAP_BCPROP_NDI_PTZ_FOCUS_SPEED,			// in: float -1.0 (focus outwards) ... +1.0 (focus inwards)
	QCAP_BCPROP_NDI_PTZ_WHITE_BALANCE_AUTO,		// in: NULL
	QCAP_BCPROP_NDI_PTZ_WHITE_BALANCE_INDOOR,	// in: NULL
	QCAP_BCPROP_NDI_PTZ_WHITE_BALANCE_OUTDOOR,	// in: NULL
	QCAP_BCPROP_NDI_PTZ_WHITE_BALANCE_ONESHOT,	// in: NULL
	QCAP_BCPROP_NDI_PTZ_WHITE_BALANCE_MANUAL,	// in: [float 0.0(not red) ... 1.0(very red), float 0.0(not blue) ... 1.0(very blue)]
	QCAP_BCPROP_NDI_PTZ_EXPOSURE_AUTO,			// in: NULL
	QCAP_BCPROP_NDI_PTZ_EXPOSURE_MANUAL,		// in: float 0.0(dark) ... 1.0(light)
	QCAP_BCPROP_NDI_KVM_IS_SUPPORTED,			// out: BOOL
	QCAP_BCPROP_NDI_KVM_SEND_LEFT_MOUSE,		// in: BOOL 0 (release), 1 (click)
	QCAP_BCPROP_NDI_KVM_SEND_MIDDLE_MOUSE,		// in: BOOL 0 (release), 1 (click)
	QCAP_BCPROP_NDI_KVM_SEND_RIGHT_MOUSE,		// in: BOOL 0 (release), 1 (click)
	QCAP_BCPROP_NDI_KVM_SEND_WHEEL_MOUSE,		// in: int 0 (vertical), 1 (horizontal)
	QCAP_BCPROP_NDI_KVM_SEND_MOUSE_POSITION,	// in: [float 0.0 (left of the screen) ... 1.0(right of the screen), float 0.0(top of the screen) ... 1.0(botton of the screen)]
	QCAP_BCPROP_NDI_KVM_SEND_KEYBOARD,			// in: BOOL 0 (release), 1 (click)   in: int (key_sym_value)
	QCAP_BCPROP_NDI_TALLY_SET,					// in: QCAP_BPROP_NDI_TALLY_VALUE
	QCAP_BCPROP_VIDEO_DECODE_FORMAT,			// out: ULONG
	QCAP_BCPROP_VIDEO_COLORSPACE_TYPE,			// out: ULONG
	QCAP_BCPROP_AUDIO_DECODE_FORMAT,			// out: ULONG
	QCAP_BCPROP_ENABLE_LOW_LATENCY,			 	// [in/out: ULONG] boolean value
	QCAP_BCPROP_ENABLE_REWIND,				 	// [in/out: ULONG] boolean value
	QCAP_BCPROP_DRM_POS_X,						// [in/out: ULONG] boolean value
	QCAP_BCPROP_DRM_POS_Y,						// [in/out: ULONG] boolean value
	QCAP_BCPROP_DRM_WIDTH,						// [in/out: ULONG] boolean value
	QCAP_BCPROP_DRM_HEIGHT,						// [in/out: ULONG] boolean value
	QCAP_BCPROP_VO_BACKEND,						// [in/out, ULONG] 0:sdl, 1:gstreamer, 2:davmf, 3:vitis
	QCAP_BCPROP_VC_IPCAM,						// in,out: BOOL
	QCAP_BCPROP_NVT_HDAL_VDEC_ID,				// [in, ULONG] 0xFFAABBCC AA=Id BB=In CC=Out
	QCAP_BCPROP_NVT_HDAL_VPRC_ID,				// [in, ULONG] 0xFFAABBCC AA=Id BB=In CC=Out
};

// file custom property
enum {
	// QCAP_FILEPROP_PIPELINE_MODE				= 804, // in,out: ULONG, DEPRECATED!
	QCAP_FILEPROP_DRM_POS_X = 100,				// [in/out: ULONG] boolean value
	QCAP_FILEPROP_DRM_POS_Y,					// [in/out: ULONG] boolean value
	QCAP_FILEPROP_DRM_WIDTH,					// [in/out: ULONG] boolean value
	QCAP_FILEPROP_DRM_HEIGHT,					// [in/out: ULONG] boolean value
};

// pat-gen custom property
enum {
	QCAP_PATGENPROP_VERSION = 0,				// [out, ULONG]
};

// callback definitions
typedef QRETURN (QCAP_EXPORT *PF_MEDIA_BROADCAST_SERVER_CALLBACK)( PVOID pServer, UINT iSessionNum, double dSampleTime, BYTE * pStreamBuffer, ULONG nStreamBufferLen, PVOID pUserData );
typedef QRETURN (QCAP_EXPORT *PF_BACKCHANNEL_BROADCAST_SERVER_PLAY_CALLBACK)( PVOID pServer, UINT iSessionNum, PVOID pUserData );
typedef QRETURN (QCAP_EXPORT *PF_BACKCHANNEL_BROADCAST_SERVER_STOP_CALLBACK)( PVOID pServer, UINT iSessionNum, PVOID pUserData );
typedef QRETURN (QCAP_EXPORT *PF_BACKCHANNEL_BROADCAST_SERVER_AUDIO_DECODED_CALLBACK)( PVOID pServer, UINT iSessionNum, double dSampleTime, BYTE * pFrameBuffer, ULONG nFrameBufferLen, PVOID pUserData );

typedef QRETURN (QCAP_EXPORT *PF_CLIENT_CONNECTED_BROADCAST_SERVER_CALLBACK)( PVOID pServer, UINT iSessionNum, PVOID pConnectionInfos, PVOID pUserData );
typedef QRETURN (QCAP_EXPORT *PF_CLIENT_DISCONNECTED_BROADCAST_SERVER_CALLBACK)( PVOID pServer, UINT iSessionNum, PVOID pConnectionInfos, PVOID pUserData );
typedef QRETURN (QCAP_EXPORT *PF_DANTE_SERVER_MESSAGE_CALLBACK)( PVOID pServer /*IN*/, UINT nVideoChannel /*IN*/, DWORD dwFlags /*IN*/, PVOID pUserData /*IN*/ );

typedef QRETURN (QCAP_EXPORT *PF_NDI_METADATA_BROADCAST_CLIENT_CALLBACK)( PVOID pClient, UINT iSessionNum, int length, int64_t timecode, const char* p_data, PVOID pUserData );
typedef QRETURN (QCAP_EXPORT *PF_NDI_DISCOVERY_SERVICE_CONNECTED_BROADCAST_CLIENT_CALLBACK)( PVOID pClient, UINT iSessionNum, BOOL bServiceConnected, CHAR * pszNDIName, CHAR * pszGroupName, PVOID pUserData );

typedef QRETURN (QCAP_EXPORT *PF_SHARE_RECORD_CALLBACK)( UINT iRecNum /*IN*/, BYTE * pStreamBuffer /*IN*/, ULONG nStreamBufferLen /*IN*/, PVOID pUserData /*IN*/ );

typedef QRETURN (QCAP_EXPORT *PF_PATGEN_FORMAT_CHANGED_CALLBACK)(PVOID pPatGen, ULONG nColorSpaceType, ULONG nVideoWidth, ULONG nVideoHeight, BOOL bVideoIsInterleaved, double dVideoFrameRate, ULONG nAudioChannels, ULONG nAudioBitsPerSample, ULONG nAudioSampleFrequency, PVOID pUserData);
typedef QRETURN (QCAP_EXPORT *PF_PATGEN_VIDEO_PREVIEW_CALLBACK)(PVOID pPatGen, double dSampleTime, BYTE * pFrameBuffer, ULONG nFrameBufferLen, PVOID pUserData);
typedef QRETURN (QCAP_EXPORT *PF_PATGEN_AUDIO_PREVIEW_CALLBACK)(PVOID pPatGen, double dSampleTime, BYTE * pFrameBuffer, ULONG nFrameBufferLen, PVOID pUserData);

typedef QRETURN (QCAP_EXPORT *PF_UDEVMON_BIND_CALLBACK)(PVOID pUdevMonitor, ULONG pDeviceInfos[3], PVOID pUserData);
typedef QRETURN (QCAP_EXPORT *PF_UDEVMON_UNBIND_CALLBACK)(PVOID pUdevMonitor, PVOID pUserData);

typedef QRETURN (QCAP_EXPORT *PF_VIDEO_FILE_CALLBACK)( PVOID pFile, double dSampleTime, BYTE * pStreamBuffer, ULONG nStreamBufferLen, BOOL bIsKeyFrame, PVOID pUserData);
typedef QRETURN (QCAP_EXPORT *PF_AUDIO_FILE_CALLBACK)( PVOID pFile, double dSampleTime, BYTE * pStreamBuffer, ULONG nStreamBufferLen, PVOID pUserData);

#pragma pack(push)
#pragma pack(1)

struct QCAP_PROPERTY_HDMI_VO_PARAM_VALUE
{
	int width;
	int height;
	int fps;
};

struct QCAP_PROPERTY_COLOR_RANGE_VALUE
{
	ULONG color_range; // QCAP_COLORRANGE_TYPE_xxx
};

struct QCAP_PROPERTY_COLOR_RANGE_RESPONSE
{
	ULONG color_range; // QCAP_COLORRANGE_TYPE_xxx
};

struct QCAP_PROPERTY_VO_PARAM_VALUE
{
	int id;
	int width;
	int height;
	int fps;
	int flags; // Bit0: progressive / interlaced, Bit1: 1000 / 1001
};

struct QCAP_PROPERTY_VO_PARAM_RESPONSE
{
	int id; // in

	// output fields
	int width;
	int height;
	int fps;
	int flags; // Bit0: progressive / interlaced, Bit1: 1000 / 1001
};

struct QCAP_PROPERTY_VO_CSC_PARAM_VALUE
{
	int id;
	uint32_t luma;
	uint32_t contrast;
	uint32_t hue;
	uint32_t saturation;
};

struct QCAP_PROPERTY_MCU_VERSION_RESPONSE
{
	BYTE ver[4];
};

struct QCAP_PROPERTY_COPY_PROTECT_KEY_VALUE
{
	uint32_t key;
};

struct QCAP_PROPERTY_COPY_PROTECT_CHECK_RESPONSE
{
	int result;
};

struct QCAP_PROPERTY_GPIO_OUTPUT_VALUE
{
	uint32_t port; // 0x90 = PORT A, 0x91 = PORT B, 0x92 = PORT C
	uint32_t pin; // 0x00 ~ 0x1F, pin number
	uint32_t value; // 0X00 = LOW, 0X01 = HIGH
};

struct QCAP_PROPERTY_LED_VALUE
{
	int index;

	unsigned char period;
	unsigned char level;
	unsigned char start;
	unsigned char end;
};

struct QCAP_PROPERTY_LED_EX_VALUE
{
	int index;

	int argc;
	unsigned char args[16];

	int async_mode;
};

struct QCAP_PROPERTY_BUTTON_RESPONSE
{
	int size;

	uint32_t state[8];
};

struct QCAP_PROPERTY_VOLUME_VALUE
{
	int index;
	int volume;

	int async_mode;
};

struct QCAP_PROPERTY_VOLUME_RESPONSE
{
	int index; // in
	int volume[8]; // out
};

struct QCAP_PROPERTY_NDI_MODE_RESPONSE
{
	int mode; // 0: no key; 1: yuan key; 2: custom key
};

struct QCAP_PROPERTY_NOISE_GATE_VALUE
{
	// 0: LineIn1; 1: LineIn2; 2: LineIn3; 3: LineIn4; 4: LineOut1
	int index;

	// 0: Off
	// 1: -34; 2: -37; 3: -40; 4: -43; 5: -46; 6: -52; 7: -58dB
	// 8: -64; 9: -67; 10: -70; 11: -73; 12: -76; 13: -82dB
	int value;

	// 0: 50; 1: 100; 2: 150; 3: 200ms
	int delay;
};

struct QCAP_PROPERTY_NDI_KEY_VALUE
{
	char* pszNDIKey;
};

struct QCAP_PROPERTY_BUTTON_EX_RESPONSE
{
	uint32_t state[4]; // 128 button state
};

struct QCAP_PROPERTY_TSENSOR_DETECT_RESPONSE
{
	double value;
};

struct QCAP_PROPERTY_MENU_VALUE
{
	int index;
	int channel;
	int size;
	unsigned char data[64];
};

struct QCAP_PROPERTY_BUTTON_EVENT_RESPONSE
{
	int scancode;
	int flags;
};

struct QCAP_BCPROP_NDI_KVM_SEND_KEYBOARD_VALUE
{
	int type; // 0: release, 1: press
	int key;
};

struct QCAP_BPROP_NDI_TALLY_VALUE
{
	BOOL on_program; // Is this currently on program output.
	BOOL on_preview; // Is this currently on preview output.
};

struct QCAP_BPROP_BACKCHANNEL_VALUE
{
	ULONG nChannels;
	ULONG nBitsPerSample;
	ULONG nSampleFrequency;

	PF_BACKCHANNEL_BROADCAST_SERVER_PLAY_CALLBACK fnPlay;
	PF_BACKCHANNEL_BROADCAST_SERVER_STOP_CALLBACK fnStop;
	PF_BACKCHANNEL_BROADCAST_SERVER_AUDIO_DECODED_CALLBACK fnAudioDecoded;
	PVOID pUserData;
};

struct QCAP_PTZ_COMMAND
{
    BYTE OnOff;
    BYTE Header;
    BYTE Address;
    BYTE Command;
    BYTE Data1;
    BYTE Data2;
    BYTE Data3;
    BYTE Data4;
};

#pragma pack(pop)

// extension function
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_INITIALIZE();
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_UNINITIALIZE();
QCAP_EXT_API double QCAP_EXPORT QCAP_GET_TIME();
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_CUSTOM_PROPERTY(ULONG nProperty, BYTE* pValue, ULONG nBytes);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_CUSTOM_PROPERTY(ULONG nProperty, BYTE* pValue, ULONG nBytes);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SYNCHRONIZED_SNAPSHOT_BMP( ULONG nFileArgs, CHAR * pszFilePathName1, PVOID pDevice1, CHAR * pszFilePathName2, PVOID pDevice2, ... );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SYNCHRONIZED_SNAPSHOT_JPG( ULONG nFileArgs, CHAR * pszFilePathName1, ULONG nQuality1, PVOID pDevice1, CHAR * pszFilePathName2, ULONG nQuality2, PVOID pDevice2, ... );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_COLORSPACE_RGB24_TO_YUY2(  BYTE * pSrcFrameBuffer, ULONG nSrcWidth, ULONG nSrcHeight, ULONG nSrcPitch, BYTE * pDstFrameBuffer, ULONG nDstWidth, ULONG nDstHeight, ULONG nDstPitch, BOOL bHorizontalMirror, BOOL bVerticalMirror );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_COLORSPACE_RGB24_TO_YV12(  BYTE * pSrcFrameBuffer, ULONG nSrcWidth, ULONG nSrcHeight, ULONG nSrcPitch, BYTE * pDstFrameBuffer, ULONG nDstWidth, ULONG nDstHeight, ULONG nDstPitch, BOOL bHorizontalMirror, BOOL bVerticalMirror );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_COLORSPACE_ARGB32_TO_YUY2( BYTE * pSrcFrameBuffer, ULONG nSrcWidth, ULONG nSrcHeight, ULONG nSrcPitch, BYTE * pDstFrameBuffer, ULONG nDstWidth, ULONG nDstHeight, ULONG nDstPitch, BOOL bHorizontalMirror, BOOL bVerticalMirror );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_COLORSPACE_ARGB32_TO_YV12( BYTE * pSrcFrameBuffer, ULONG nSrcWidth, ULONG nSrcHeight, ULONG nSrcPitch, BYTE * pDstFrameBuffer, ULONG nDstWidth, ULONG nDstHeight, ULONG nDstPitch, BOOL bHorizontalMirror, BOOL bVerticalMirror );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_COLORSPACE_YUY2_TO_UYVY(   BYTE * pSrcFrameBuffer, ULONG nSrcWidth, ULONG nSrcHeight, ULONG nSrcPitch, BYTE * pDstFrameBuffer, ULONG nDstWidth, ULONG nDstHeight, ULONG nDstPitch, BOOL bHorizontalMirror, BOOL bVerticalMirror );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_COLORSPACE_YV12_TO_UYVY(   BYTE * pSrcFrameBuffer, ULONG nSrcWidth, ULONG nSrcHeight, ULONG nSrcPitch, BYTE * pDstFrameBuffer, ULONG nDstWidth, ULONG nDstHeight, ULONG nDstPitch, BOOL bHorizontalMirror, BOOL bVerticalMirror );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_COLORSPACE_NV12_TO_UYVY(   BYTE * pSrcFrameBuffer, ULONG nSrcWidth, ULONG nSrcHeight, ULONG nSrcPitch, BYTE * pDstFrameBuffer, ULONG nDstWidth, ULONG nDstHeight, ULONG nDstPitch, BOOL bHorizontalMirror, BOOL bVerticalMirror );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_COLORSPACE_CONVERT(ULONG nSrcColorSpaceType, BYTE * pSrcFrameBuffer, ULONG nSrcWidth, ULONG nSrcHeight, ULONG nSrcPitch, ULONG nDstColorSpaceType, BYTE * pDstFrameBuffer, ULONG nDstWidth, ULONG nDstHeight, ULONG nDstPitch, BOOL bHorizontalMirror, BOOL bVerticalMirror );
/*
 * @brief Crops a video frame.
 * @param[in] pSrcFrameBuffer Source frame buffer (RC-only, must be a valid refcounted buffer pointer).
 * @param[in] nSrcFrameBufferLen Reserved source-buffer tag value 0xFFFFCAFE.
 * @param[in] nCropX X coordinate of crop rectangle.
 * @param[in] nCropY Y coordinate of crop rectangle.
 * @param[in] nCropW Width of crop rectangle.
 * @param[in] nCropH Height of crop rectangle.
 * @param[out] pDstFrameBuffer Destination frame buffer (RC-only, must be a valid refcounted buffer pointer).
 * @param[in] nDstFrameBufferLen Reserved destination-buffer tag value 0xFFFFCAFE.
 */
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CROP_VIDEO_FRAME( BYTE* pSrcFrameBuffer, ULONG nSrcFrameBufferLen, ULONG nCropX, ULONG nCropY, ULONG nCropW, ULONG nCropH, BYTE* pDstFrameBuffer, ULONG nDstFrameBufferLen );

/*
 * @brief Creates a legacy tagged reference-counted buffer handle.
 * @param[out] ppBuffer Receives the opaque buffer handle as BYTE*.
 * @param[out] pBufferLen Receives the reserved tag value 0xFFFFCAFE.
 * @note The caller owns the returned handle and must call QCAP_DESTROY_BUFFER
 *       or the matching QCAP_BUFFER_RELEASE operation when done.
 */
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_BUFFER( BYTE** ppBuffer, ULONG* pBufferLen );

/*
 * @brief Releases the caller's reference on a reference-counted buffer handle.
 * @param[in] pBuffer The buffer handle pointer.
 * @param[in] nBufferLen Must be the reserved tag value 0xFFFFCAFE.
 */
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_DESTROY_BUFFER( BYTE* pBuffer, ULONG nBufferLen );

/* reference counted buffer */

/*
 * @brief Converts a legacy tagged pair to an opaque RC handle.
 * @param[in] pBuffer The buffer handle pointer.
 * @param[in] nBufferLen Must be the reserved tag value 0xFFFFCAFE.
 * @return The opaque RC handle, or NULL for a null pointer or wrong tag.
 * @warning A matching tag does not validate pointer origin or lifetime, and
 *          this function does not acquire ownership.
 */
QCAP_EXT_API PVOID QCAP_EXPORT QCAP_BUFFER_GET_RCBUFFER( BYTE * pBuffer, ULONG nBufferLen );

/*
 * @brief Pins the media-data lifetime and returns its pointer.
 * @param[in] pRCBuffer The opaque RC handle.
 * @return Pointer to media data (usually QCAP_AV_FRAME or QCAP_AV_PACKET), or
 *         NULL if it is no longer available.
 * @note Despite the name, this is not a mutex and provides no read/write
 *       exclusion. Every successful call must have a matching unlock.
 */
QCAP_EXT_API PVOID QCAP_EXPORT QCAP_RCBUFFER_LOCK_DATA( PVOID pRCBuffer );

/*
 * @brief Releases one media-data lifetime pin.
 * @param[in] pRCBuffer The opaque RC handle.
 * @note Stop using the media-data pointer before this call.
 */
QCAP_EXT_API void QCAP_EXPORT QCAP_RCBUFFER_UNLOCK_DATA( PVOID pRCBuffer );

/*
 * @brief Acquires a shared reference to an opaque RC handle.
 * @param[in] pRCBuffer The opaque RC handle.
 * @note Every acquired reference must be released with
 *       QCAP_RCBUFFER_RELEASE. Do not substitute QCAP_BUFFER_ADD_REF; the two
 *       API pairs represent different ownership operations.
 */
QCAP_EXT_API void QCAP_EXPORT QCAP_RCBUFFER_ADD_REF( PVOID pRCBuffer );

/*
 * @brief Releases a shared reference to an opaque RC handle.
 * @param[in] pRCBuffer The opaque RC handle.
 * @note Must match QCAP_RCBUFFER_ADD_REF or an ownership transfer documented by
 *       the producer. Do not substitute QCAP_BUFFER_RELEASE.
 */
QCAP_EXT_API void QCAP_EXPORT QCAP_RCBUFFER_RELEASE( PVOID pRCBuffer );

/*
 * @brief Historical declaration for creating a video/audio frame-buffer handle.
 * @param[out] ppBuffer Pointer to the allocated buffer handle, cast as BYTE*.
 * @param[out] pBufferLen Receives the reserved tag value 0xFFFFCAFE.
 * @deprecated Retained for compatibility. New code should use typed qcap2
 *             buffer/pool APIs.
 * @warning Verify that the target SDK actually exports this legacy symbol.
 */
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_FRAME_BUFFER( BYTE** ppBuffer, ULONG* pBufferLen );

/*
 * @brief Historical declaration for releasing a frame-buffer handle.
 * @param[in] pBuffer The buffer handle pointer.
 * @param[in] nBufferLen Must be the reserved tag value 0xFFFFCAFE.
 * @deprecated This legacy declaration is retained for compatibility. New code
 *             should use typed qcap2 buffer/pool APIs.
 * @note `DESOTRY` is a historical spelling error retained for compatibility;
 *       use this exact spelling when targeting the legacy declaration.
 */
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_DESOTRY_FRAME_BUFFER( BYTE * pBuffer, ULONG nBufferLen );

// code-block lock
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_BLOCK_LOCK( PVOID* ppBlockLock );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_DESTROY_BLOCK_LOCK( PVOID pBlockLock );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GRANT_BLOCK_LOCK( PVOID pBlockLock, BOOL bGrant );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_ENTER_BLOCK_LOCK( PVOID pBlockLock );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_LEAVE_BLOCK_LOCK( PVOID pBlockLock );

// A/V frame pool
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_VIDEO_FRAME_POOL( ULONG nColorSpaceType, ULONG nFrameWidth, ULONG nFrameHeight, ULONG nFrames, PVOID* ppFramePool );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_VIDEO_FRAME_POOL_EX( ULONG nColorSpaceType, ULONG nFrameWidth, ULONG nFrameHeight, ULONG nFrames, BOOL bMapped, PVOID* ppFramePool );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_AUDIO_FRAME_POOL( ULONG nChannels, ULONG nBitsPerSample, ULONG nSampleFrequency, ULONG nFrameSize, ULONG nFrames, PVOID* ppFramePool );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_AUDIO_FRAME_POOL_EX( ULONG nChannels, ULONG nSampleFmt, ULONG nSampleFrequency, ULONG nFrameSize, ULONG nFrames, PVOID* ppFramePool );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_FRAME_BUFFER( PVOID pFramePool, BYTE** ppFrameBuffer, ULONG* pFrameBufferSize );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_DESTROY_FRAME_POOL( PVOID pFramePool );

// audio resampler
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_AUDIO_RESAMPLER( ULONG nSrcChannels, ULONG nSrcBitsPerSample, ULONG nSrcSampleFrequency, ULONG nDstChannels, ULONG nDstBitsPerSample, ULONG nDstSampleFrequency, PVOID* ppAudioResampler );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_DESTROY_AUDIO_RESAMPLER( PVOID pAudioResampler );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_AUDIO_RESAMPLER_SET_AUDIO_BUFFER( PVOID pAudioResampler, BYTE * pSrcFrameBuffer, ULONG nSrcFrameBufferLen, double dSampleTime);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_AUDIO_RESAMPLER_GET_RESAMPLED_AUDIO_BUFFER( PVOID pAudioResampler, BYTE * pDstFrameBuffer, ULONG nDstFrameBufferLen, double * pSampleTime);

// stream buffer pool
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_STREAM_BUFFER_POOL( ULONG nCount, PVOID* ppStreamBufferPool );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_STREAM_BUFFER( PVOID pStreamBufferPool, BYTE** ppStreamBuffer, ULONG* pStreamBufferLen );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_DESTROY_STREAM_BUFFER_POOL( PVOID pStreamBufferPool );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_COPY_NATIVE_STREAM_BUFFER( BYTE* pDstStreamBuffer, ULONG nDstStreamBufferLen, BYTE* pSrcStreamBuffer, ULONG nSrcStreamBufferLen, BOOL bIsKeyFrame, double dSrcSampleTime );

// audio delay queue
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_AUDIO_DELAY_QUEUE( PVOID* ppFrameQueue );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_AUDIO_DELAY( PVOID pFrameQueue, ULONG nMaxDelayInMS );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_PUSH_AUDIO_FRAME_BUFFER( PVOID pFrameQueue, BYTE* pSrcFrameBuffer, ULONG nSrcFrameBufferSize, BYTE** ppDstFrameBuffer, ULONG* pDstFrameBufferSize);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_DESTROY_AUDIO_DELAY_QUEUE( PVOID pFrameQueue );

// animation extension
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_STEP_ANIMATION_CLIP2( PVOID pClip, UINT * pFrameNum, BYTE * pFrameBuffer, ULONG nFrameBufferLen);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_ANIMATION_CLIP_BKCOLOR_PROPERTY( PVOID pClip, float* pR, float* pG, float* pB );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_ANIMATION_CLIP_BKCOLOR_PROPERTY( PVOID pClip, float r, float g, float b );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_ANIMATION_CLIP_SPRITE_SCALE_STYLE( PVOID pClip, UINT iSpriteNum, ULONG* pScaleStyle ); // QCAP_SCALE_STYLE_STRETCH / QCAP_SCALE_STYLE_FIT / QCAP_SCALE_STYLE_FILL
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_ANIMATION_CLIP_SPRITE_SCALE_STYLE( PVOID pClip, UINT iSpriteNum, ULONG nScaleStyle ); // QCAP_SCALE_STYLE_STRETCH / QCAP_SCALE_STYLE_FIT / QCAP_SCALE_STYLE_FILL
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_ANIMATION_CLIP_SPRITE_BKCOLOR( PVOID pClip, UINT iSpriteNum, float* pR, float* pG, float* pB );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_ANIMATION_CLIP_SPRITE_BKCOLOR( PVOID pClip, UINT iSpriteNum, float r, float g, float b );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_ANIMATION_CLIP_SPRITE_STATIC( PVOID pClip, UINT iSpriteNum, BOOL* pStatic );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_ANIMATION_CLIP_SPRITE_STATIC( PVOID pClip, UINT iSpriteNum, BOOL bStatic );

// broadcast server extension
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_BROADCAST_TS_OVER_UDP_SERVER_EX( UINT iSvrNum /*0 ~ 63*/, CHAR * pszURL, PVOID * ppServer, CHAR * pszNetworkAdapterIP DEFVAL( NULL), ULONG nServiceID DEFVAL( 1), CHAR * pszServiceName DEFVAL( NULL), CHAR * pszServiceProviderName DEFVAL( NULL), BOOL bEnableCBRMode DEFVAL( TRUE), ULONG nTransferBitRate DEFVAL( 0 /*0 = FREE*/), ULONG nOriginalNetworkID DEFVAL( 1), ULONG nTransportStreamID DEFVAL( 1), ULONG nPMT_PID DEFVAL( 4096), ULONG nPCR_PID DEFVAL( 4097), ULONG nVideo_PID DEFVAL( 256), ULONG nAudio_PID DEFVAL( 257), ULONG nVideo_CodecID DEFVAL( 0 /*0 = AUTO*/), ULONG nAudio_CodecID DEFVAL( 0 /*0 = AUTO*/), ULONG nPCRInterval DEFVAL( 0 /*ms*/ ), ULONG nTTL DEFVAL( 7 )); // TOTOAL SESSION = 1
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_BROADCAST_TS_OVER_SRT_SERVER_EX( UINT iSvrNum /*0 ~ 63*/, PVOID * ppServer, ULONG nNetworkPort_SRT DEFVAL( 1234 ), CHAR * pszPassPhrase DEFVAL( NULL ), ULONG nPBKeyLen DEFVAL( 16 ),  CHAR * pszNetworkAdapterIP DEFVAL(  NULL ), ULONG nServiceID DEFVAL(1) , CHAR * pszServiceName DEFVAL( NULL ), CHAR * pszServiceProviderName DEFVAL( NULL ) , ULONG nTransferBitRate DEFVAL(0) , ULONG nOriginalNetworkID DEFVAL(1) , ULONG nTransportStreamID DEFVAL( 1), ULONG nPMT_PID DEFVAL( 4096 ), ULONG nPCR_PID DEFVAL( 4097 ), ULONG nVideo_PID DEFVAL(256), ULONG nAudio_PID DEFVAL(257), ULONG nVideo_CodecID DEFVAL(0) /*0 = AUTO*/, ULONG nAudio_CodecID DEFVAL(0) /*0 = AUTO*/, ULONG nPCRInterval DEFVAL(0) /*ms*/, ULONG nSRTLatency DEFVAL(120 ) /*ms*/, ULONG nSRTBandwidth DEFVAL(25 /*100%*/) );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_BROADCAST_TS_OVER_SRT_WEB_PORTAL_SERVER_EX( UINT iSvrNum /*0 ~ 63*/, CHAR * pszURL, PVOID * ppServer, CHAR * pszPassPhrase DEFVAL( NULL ),  ULONG nPBKeyLen DEFVAL( 16 ), ULONG nServiceID DEFVAL( 1 ), CHAR * pszServiceName DEFVAL(NULL), CHAR * pszServiceProviderName DEFVAL(NULL), ULONG nTransferBitRate DEFVAL(0) /*0 DEFVAL(FREE*/, ULONG nOriginalNetworkID DEFVAL(1), ULONG nTransportStreamID DEFVAL(1), ULONG nPMT_PID DEFVAL(4096), ULONG nPCR_PID DEFVAL(4097), ULONG nVideo_PID DEFVAL(256), ULONG nAudio_PID DEFVAL(257), ULONG nVideo_CodecID DEFVAL(0) /*0 = AUTO*/, ULONG nAudio_CodecID DEFVAL( 0 ) /*0 = AUTO*/, ULONG nPCRInterval DEFVAL( 0 ) /*ms*/, ULONG nSRTLatency DEFVAL( 120) /*ms*/, ULONG nSRTOverheadBandwidth DEFVAL( 25 ) /*100%*/ ); // TOTOAL SESSION = 1
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_BROADCAST_RTP_SERVER( UINT iSvrNum /*0 ~ 63*/, CHAR* pszIPorSDP, PVOID * ppServer, ULONG nNetworkPort DEFVAL( (ULONG)-1 ));
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_BROADCAST_QCAP_SERVER( UINT iSvrNum /*0 ~ 63*/, ULONG nNetworkPort, PVOID * ppServer );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_MEDIA_BROADCAST_SERVER_CALLBACK( PVOID pServer, UINT iSessionNum, PF_MEDIA_BROADCAST_SERVER_CALLBACK pCB, PVOID pUserData );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_CLIENT_CONNECTED_BROADCAST_SERVER_CALLBACK( PVOID pServer, UINT iSessionNum, PF_CLIENT_CONNECTED_BROADCAST_SERVER_CALLBACK pCB, PVOID pUserData );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_CLIENT_DISCONNECTED_BROADCAST_SERVER_CALLBACK( PVOID pServer, UINT iSessionNum, PF_CLIENT_DISCONNECTED_BROADCAST_SERVER_CALLBACK pCB, PVOID pUserData );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_BROADCAST_SERVER_INFO( PVOID pServer, UINT iSessionNum, ULONG * pConnections );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_BROADCAST_SERVER_CUSTOM_PROPERTY( PVOID pServer, ULONG nProperty, BYTE* pValue, ULONG nBytes);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_BROADCAST_SERVER_CUSTOM_PROPERTY( PVOID pServer, ULONG nProperty, BYTE* pValue, ULONG* pBytes);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_SESSION_BROADCAST_SERVER_CUSTOM_PROPERTY( PVOID pServer, UINT iSessionNum, ULONG nProperty, BYTE* pValue, ULONG nBytes);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_SESSION_BROADCAST_SERVER_CUSTOM_PROPERTY( PVOID pServer, UINT iSessionNum, ULONG nProperty, BYTE* pValue, ULONG* pBytes);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_ID3_BROADCAST_SERVER_COMPRESSION_BUFFER( PVOID pServer, UINT iSessionNum, BYTE * pStreamBuffer, ULONG nStreamBufferLen, double dSampleTime DEFVAL(0.0));

// broadcast client extension
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_BROADCAST_SRT_CLIENT_EX( UINT iCliNum /*0 ~ 63*/, CHAR * pszURL, PVOID * ppClient, ULONG nSRTLatency DEFVAL(120) , ULONG nSRTOverheadBandwidth DEFVAL(25), ULONG nDecoderType DEFVAL( QCAP_DECODER_TYPE_SOFTWARE) , HWND hAttachedWindow DEFVAL(NULL), BOOL bThumbDraw DEFVAL(FALSE), BOOL bMaintainAspectRatio DEFVAL(FALSE), CHAR * pszPassPhrase DEFVAL(NULL),  ULONG nPBKeyLen DEFVAL(16));
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_BROADCAST_CLIENT_CUSTOM_PROPERTY_EX( PVOID pClient, ULONG nProperty, BYTE* pValue, ULONG nBytes);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_BROADCAST_CLIENT_CUSTOM_PROPERTY_EX( PVOID pClient, ULONG nProperty, BYTE* pValue, ULONG* pBytes);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_BROADCAST_NDI_CLIENT_ENUMERATION1( CHAR * * ppszNDIName /*OUT*/, CHAR * * ppszGroupName /*OUT*/, BOOL bShowLocalSource DEFVAL(TRUE) /*IN*/, CHAR* pszGroups DEFVAL(NULL)/*IN*/, CHAR* pszExtraIp DEFVAL(NULL)/*IN*/, BOOL bNext DEFVAL( FALSE /*IN*/ ));
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_BROADCAST_NDI_CLIENT_ENUMERATION2( CHAR * * ppszNDIName /*OUT*/, CHAR * * ppszGroupName /*OUT*/, CHAR * * ppszIp /*OUT*/, BOOL bShowLocalSource DEFVAL(TRUE) /*IN*/, CHAR* pszGroups DEFVAL(NULL)/*IN*/, CHAR* pszExtraIp DEFVAL(NULL)/*IN*/, BOOL bNext DEFVAL( FALSE /*IN*/ ));
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_NDI_METADATA_BROADCAST_CLIENT_CALLBACK( PVOID pClient /*IN*/, PF_NDI_METADATA_BROADCAST_CLIENT_CALLBACK pCB /*IN*/, PVOID pUserData /*IN*/ );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_BROADCAST_NDI_CLIENT1( UINT iCliNum /*IN*/ /*0 ~ 63*/, CHAR * pszDiscoveryServiceIp /*IN*/, PVOID * ppClient /*OUT*/, BOOL bEnableNDIProxy DEFVAL( FALSE /*IN*/), ULONG nDecoderType DEFVAL( QCAP_DECODER_TYPE_SOFTWARE /*IN*/), HWND hAttachedWindow DEFVAL( NULL /*IN*/), BOOL bThumbDraw DEFVAL( FALSE /*IN*/), BOOL bMaintainAspectRatio DEFVAL( FALSE /*IN*/ ));
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_NDI_DISCOVERY_SERVICE_CONNECTED_BROADCAST_CLIENT_CALLBACK( PVOID pClient /*IN*/, PF_NDI_DISCOVERY_SERVICE_CONNECTED_BROADCAST_CLIENT_CALLBACK pCB /*IN*/, PVOID pUserData /*IN*/ );

// share record extension
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_VIDEO_SHARE_RECORD_CUSTOM_PROPERTY( UINT iRecNum, ULONG nProperty, BYTE* pValue, ULONG nBytes);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_VIDEO_SHARE_RECORD_CUSTOM_PROPERTY( UINT iRecNum, ULONG nProperty, BYTE* pValue, ULONG* pBytes);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_AUDIO_MX_SHARE_RECORD_VOLUME(UINT iRecNum, ULONG nTracks, UINT iMixNum, ULONG  nVolume);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_AUDIO_MX_SHARE_RECORD_VOLUME(UINT iRecNum, ULONG nTracks, UINT iMixNum, ULONG* pVolume);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_SHARE_RECORD_CALLBACK( UINT iRecNum /*IN*/, PF_SHARE_RECORD_CALLBACK pCB /*IN*/, PVOID pUserData /*IN*/ );
// Adds a named MP4 chapter at the first muxed video sample at or after dSampleTime.
// The chapter name is copied before this function returns. Requires BUILD_WITH_ZZMP4_CHAPTER.
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_VIDEO_SHARE_RECORD_CHAPTER(UINT iRecNum, CHAR * pszChapter, double dSampleTime);

// file extension
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_FILE_CUSTOM_PROPERTY_EX( PVOID pFile, ULONG nProperty, BYTE * pValue, ULONG* pBytes );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_FILE_CUSTOM_PROPERTY_EX( PVOID pFile, ULONG nProperty, BYTE * pValue, ULONG nBytes );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_VIDEO_FILE_CALLBACK( PVOID pFile, PF_VIDEO_FILE_CALLBACK pCB, PVOID pUserData );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_AUDIO_FILE_CALLBACK( PVOID pFile, PF_AUDIO_FILE_CALLBACK pCB, PVOID pUserData );


// video frame ring buffer
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_RINGBUFFER(ULONG nMaxSize, PVOID* ppRingBuffer);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_DESTROY_RINGBUFFER(PVOID pRingbuffer);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_ENQUEUE_RINGBUFFER(PVOID pRingbuffer, BYTE* pFrameBuffer, ULONG nFrameBufferLen);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_RINGBUFFER_FRAME_BUFFER(PVOID pRingbuffer, INT nOffset, BYTE** ppFrameBuffer, ULONG* pFrameBufferLen);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_RESET_RINGBUFFER(PVOID pRingbuffer);

// video frame compositor
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_COMPOSITOR(PVOID* ppCompositor);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_DESTROY_COMPOSITOR(PVOID pCompositor);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_BEGIN_COMPOSITE(PVOID pCompositor, BYTE* pFrameBuffer, ULONG nFrameBufferLen);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_COMPOSITE_VIDEO_FRAME(PVOID pCompositor, BYTE* pSrcFrameBuffer, ULONG nSrcFrameBufferLen,
	ULONG nSrcCropX, ULONG nSrcCropY, ULONG nSrcCropW, ULONG nSrcCropH,
	ULONG nDstCropX, ULONG nDstCropY, ULONG nDstCropW, ULONG nDstCropH);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_BLENDOVER_VIDEO_FRAME(PVOID pCompositor, BYTE* pSrcFrameBuffer, ULONG nSrcFrameBufferLen,
	ULONG nSrcCropX, ULONG nSrcCropY, ULONG nSrcCropW, ULONG nSrcCropH,
	ULONG nDstCropX, ULONG nDstCropY, ULONG nDstCropW, ULONG nDstCropH);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_END_COMPOSITE(PVOID pCompositor);

// dante server
enum {
	QCAP_DANTE_FLAG_VIDEO = 0x00000001,
	QCAP_DANTE_FLAG_AUDIO = 0x00000002,
	QCAP_DANTE_FLAG_RX = 0x00000004,
	QCAP_DANTE_FLAG_TX = 0x00000008,
	QCAP_DANTE_FLAG_UNICAST = 0x00000010,
	QCAP_DANTE_FLAG_MULTICAST = 0x00000020,
	QCAP_DANTE_FLAG_CONNECTED = 0x00000040,
	QCAP_DANTE_FLAG_DISCONNECT = 0x00000080,
	QCAP_DANTE_FLAG_NO_VIDEO_PACKET = 0x00000100,
};
enum {
	QCAP_DANTE_MESSAGE_EXT_NOT_CONNECTED = 0,
	QCAP_DANTE_MESSAGE_OK = 1,
};

typedef QRETURN (QCAP_EXPORT *PF_DANTE_SERVER_MESSAGE_CALLBACK)( PVOID pServer /*IN*/, UINT nVideoChannel /*IN*/, DWORD dwFlags /*IN*/, PVOID pUserData /*IN*/ );
typedef QRETURN (QCAP_EXPORT *PF_DANTE_SERVER_TX_AUDIO_REQUEST_CALLBACK)( PVOID pServer /*IN*/, double dSampleTime /*IN*/, PVOID pUserData /*IN*/ );
typedef QRETURN (QCAP_EXPORT *PF_DANTE_SERVER_NO_RESPOND_CALLBACK)( PVOID pServer /*IN*/, BOOL bActivate, PVOID pUserData /*IN*/ );
typedef QRETURN (QCAP_EXPORT *PF_DANTE_SERVER_MESSAGE_CALLBACK1)( PVOID pServer /*IN*/, UINT nVideoChannel /*IN*/, DWORD dwFlags /*IN*/, CHAR* pszTransmitterIp /*IN*/, CHAR* pszReceiverIp /*IN*/, UINT nPort /*IN*/, PVOID pUserData /*IN*/ );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_DANTE_SERVER(PVOID * ppServer /*OUT*/);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_START_DANTE_SERVER(PVOID pServer /*IN*/);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_STOP_DANTE_SERVER(PVOID pServer /*IN*/);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_DESTROY_DANTE_SERVER(PVOID pServer /*IN*/);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SEND_DANTE_SERVER_MESSAGE( PVOID pServer, ULONG nVideoTxChannel, ULONG nVideoRxChannel, ULONG nMessage);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_DANTE_SENDER(PVOID pServer /*IN*/, UINT iSvrNum /*IN*/ /*0 ~ 63*/, PVOID * ppSender /*OUT*/, UINT nVideoChannel /*IN*/, UINT nAudioChannel /*IN*/, CHAR * pszNetworkAdapterIP DEFVAL( NULL /*IN*/));
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_DANTE_RECEIVER(PVOID pServer /*IN*/, UINT nVideoChannel /*IN*/, UINT nAudioChannel /*IN*/, UINT iCliNum /*IN*/ /*0 ~ 63*/, PVOID * ppClient /*OUT*/, ULONG nDecoderType DEFVAL( QCAP_DECODER_TYPE_SOFTWARE /*IN*/), HWND hAttachedWindow DEFVAL( NULL /*IN*/), BOOL bThumbDraw DEFVAL( FALSE /*IN*/), BOOL bMaintainAspectRatio DEFVAL( FALSE /*IN*/ ));
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_DANTE_SERVER_CUSTOM_PROPERTY(PVOID pServer /*IN*/, ULONG nTotalVideoSenderChannels, ULONG nTotalVideoReceiverChannels);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_DANTE_SERVER_CUSTOM_PROPERTY(PVOID pServer /*IN*/, ULONG* nTotalVideoSenderChannels /*OUT*/, ULONG* nTotalVideoReceiverChannels /*OUT*/, ULONG* nTotalAudioSenderChannels /*OUT*/, ULONG* nTotalAudioReceiverChannels /*OUT*/);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_DANTE_SERVER_MESSAGE_CALLBACK( PVOID pServer /*IN*/, PF_DANTE_SERVER_MESSAGE_CALLBACK pCB /*IN*/, PVOID pUserData /*IN*/ );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_DANTE_SERVER_TX_AUDIO_REQUEST_CALLBACK( PVOID pServer /*IN*/, PF_DANTE_SERVER_TX_AUDIO_REQUEST_CALLBACK pCB /*IN*/, PVOID pUserData /*IN*/ );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_DANTE_SERVER_NO_RESPOND_CALLBACK( PVOID pServer /*IN*/, PF_DANTE_SERVER_NO_RESPOND_CALLBACK pCB /*IN*/, PVOID pUserData /*IN*/ );
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_DANTE_SERVER_MESSAGE_CALLBACK1( PVOID pServer /*IN*/, PF_DANTE_SERVER_MESSAGE_CALLBACK1 pCB /*IN*/, PVOID pUserData /*IN*/ );

// ndi server
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_BROADCAST_NDI_SERVER2( UINT iSvrNum, ULONG nTotalSessions, PVOID * ppServer, CHAR * pszNDIName, CHAR * pszGroupName, BOOL bEnablePTZ, CHAR * pszConnectionMetadata);

// pattern generator
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_PATGEN(PVOID* ppPatGen);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_DESTROY_PATGEN(PVOID pPatGen);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_RUN_PATGEN(PVOID pPatGen);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_STOP_PATGEN(PVOID pPatGen);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_PATGEN_VIDEO_OUTPUT_FORMAT(PVOID pPatGen, ULONG nColorSpaceType, ULONG nWidth, ULONG nHeight, BOOL bIsInterleaved, double dFrameRate);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_PATGEN_AUDIO_OUTPUT_FORMAT(PVOID pPatGen, ULONG nChannels, ULONG nBitsPerSample, ULONG nSampleFrequency);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_PATGEN_CUSTOM_PROPERTY(PVOID pPatGen, ULONG nProperty, BYTE* pValue, ULONG nBytes);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_PATGEN_CUSTOM_PROPERTY(PVOID pPatGen, ULONG nProperty, BYTE* pValue, ULONG nBytes);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_PATGEN_FORMAT_CHANGED_CALLBACK(PVOID pPatGen, PF_PATGEN_FORMAT_CHANGED_CALLBACK pCB, PVOID pUserData);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_PATGEN_VIDEO_PREVIEW_CALLBACK(PVOID pPatGen, PF_PATGEN_VIDEO_PREVIEW_CALLBACK pCB, PVOID pUserData);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_PATGEN_AUDIO_PREVIEW_CALLBACK(PVOID pPatGen, PF_PATGEN_AUDIO_PREVIEW_CALLBACK pCB, PVOID pUserData);

// udevmon
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CREATE_UDEVMON(PVOID* ppUdevMonitor);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_DESTROY_UDEVMON(PVOID pUdevMonitor);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_RUN_UDEVMON(PVOID pUdevMonitor);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_STOP_UDEVMON(PVOID pUdevMonitor);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_UDEVMON_BIND_CALLBACK(PVOID pUdevMonitor, PF_UDEVMON_BIND_CALLBACK pCB, PVOID pUserData);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_REGISTER_UDEVMON_UNBIND_CALLBACK(PVOID pUdevMonitor, PF_UDEVMON_UNBIND_CALLBACK pCB, PVOID pUserData);

// onvif extension
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_COMMUNICATION_SERVER_AUDIO_DECODER(PVOID pServer, UINT iAudDecNum, ULONG * pDecoderFormat, ULONG * pChannels, ULONG * pBitsPerSample, ULONG * pSampleFrequency, ULONG * pBitRate_Min, ULONG * pBitRate_Max, ULONG * pBitRate_Default);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_SET_COMMUNICATION_SERVER_AUDIO_DECODER(PVOID pServer, UINT iAudEncNum, ULONG nEncoderFormat, ULONG nChannels, ULONG nBitsPerSample, ULONG nSampleFrequency, ULONG nBitRate_Min, ULONG nBitRate_Max, ULONG nBitRate_Default);
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_GET_COMMUNICATION_EMULATOR_SERVER_INFO_EX(PVOID pEmulator, UINT iSvrNum, CHAR * * ppszNetworkIP, ULONG * pNetworkPort, CHAR * * ppszNetworkURL, CHAR * * ppszName, CHAR * * ppszLocation);

// camera extension
QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_CAMERA_ENUMERATION2( CHAR * * ppszCameraDevName /*OUT*/, BOOL bNext /*IN*/, ULONG pEncoderFormat /*IN*/, BOOL * bIncluded /*OUT*/);

// cudamem alloc
enum {
	QCAP_CUDAHOST_ALLOC_DEFAULT = 0x00000000,
	QCAP_CUDAHOST_ALLOC_PORTABLE = 0x00000001,
	QCAP_CUDAHOST_ALLOC_MAPPED = 0x00000002,
	QCAP_CUDAHOST_ALLOC_WRITE_COMBINED = 0x00000004,
};

QCAP_EXT_API QRESULT QCAP_EXPORT QCAP_ALLOC_VIDEO_GPUDIRECT_PREVIEW_BUFFER_EX( PVOID pDevice /*IN*/, BYTE * * ppFrameBuffer /*OUT*/, ULONG nFrameBufferLen /*IN*/, ULONG nAllocMode = QCAP_CUDAHOST_ALLOC_MAPPED );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP_COMMON_H__
