#ifndef __QCAP2_V4L2_IOCTL_H__
#define __QCAP2_V4L2_IOCTL_H__

#include <stdint.h>
#include <linux/videodev2.h>

#define QCAP2_V4L2_CID_CONTROL_MDIN	(V4L2_CID_MPEG_BASE + 275)

enum qcap2_v4l2_ctrl_mdin_ctrl_type_t {
	qcap2_v4l2_ctrl_mdin_ctrl_type_panel = 1,
	qcap2_v4l2_ctrl_mdin_ctrl_type_pip = 2,
	qcap2_v4l2_ctrl_mdin_ctrl_type_key = 3,
};

#pragma pack(push)
#pragma pack(1)

struct qcap2_v4l2_ctrl_mdin_panel_t {
	uint8_t ctrl_type; // => qcap2_v4l2_ctrl_mdin_ctrl_type_panel

	uint8_t	VideoOutMode;		//[3:0]: Output Composition (0: Full, 1: PIP, 2: Multiview, 3: PBP, 4: 1+3, 5: key mode, 6: PBP(T/B)), [6:4]: Rsv, [7]: HW update
	uint8_t	SelPort;			//0: CH1, 1: CH2, 2: CH3, 3: CH4 ([7:4]: Rsv., [3:2]: IN2, [1:0]: IN1)
	uint8_t	AudioOutMode;		//0: Embedded-in, 1: Line-in, 2: Mix (this setting can be overridden by MixerParam)
	uint8_t	AudioVolume;		//0-100**** (original range is 0-127)
};

struct qcap2_v4l2_ctrl_mdin_pip_t {
	uint8_t ctrl_type; // => qcap2_v4l2_ctrl_mdin_ctrl_type_pip

	uint8_t		PicSize;		//0: 640x480, 1: 720x480, 2: 720x576, 3: 1280x720, 4: 1680x720, 5: 1920x1080
	uint8_t		PipCtrl;		//[0]: enable manual control of sub video size, [1]: enable manual control of main video size
	uint16_t	PositionXs;		//0-3840, sub video x position
	uint16_t	PositionYs;		//0-2160, sub video y position
	uint8_t  	SizeWs;			//32-192, sub video width (actual width = SizeWs * 20, actual range: 320-3840)
	uint8_t		SizeHs;			//24-108, sub video height (actual height = SizeHs * 20, actual range: 480-2160)
	uint16_t	PositionXm;		//0-3840, main video x position
	uint16_t	PositionYm;		//0-2160, main video y position
	uint8_t  	SizeWm;			//32-192, main video width (actual width = SizeWm * 20, actual range: 320-3840)
	uint8_t		SizeHm;			//24-108, main video height (actual height = SizeHm * 20, actual range: 480-2160)
};

struct qcap2_v4l2_ctrl_mdin_key_t {
	uint8_t ctrl_type; // => qcap2_v4l2_ctrl_mdin_ctrl_type_key

	uint8_t		KeyMode;			//0: key color is green (default), 1: key color is blue, 2: Manually set by UskHue
	uint8_t		UskClipRatio;		//0-20
	uint16_t	LUTLowerRange;		//0-1023
	uint16_t	LUTUpperRange;		//0-1023
	uint16_t	UskHue;				//0-360, default: 120 (green)
	uint16_t	UskAngle;			//0-360, default: 120
	uint16_t	UskTolerance;		//0-4095, default: 3685
	uint16_t	UskSoftness;		//0-4095, default: 409
};

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int qcap2_video_sink_set_panel(int nFd, qcap2_v4l2_ctrl_mdin_panel_t* pParams);
int qcap2_video_sink_get_panel(int nFd, qcap2_v4l2_ctrl_mdin_panel_t* pParams);

int qcap2_video_sink_set_pip(int nFd, qcap2_v4l2_ctrl_mdin_pip_t* pParams);
int qcap2_video_sink_get_pip(int nFd, qcap2_v4l2_ctrl_mdin_pip_t* pParams);

int qcap2_video_sink_set_key(int nFd, qcap2_v4l2_ctrl_mdin_key_t* pParams);
int qcap2_video_sink_get_key(int nFd, qcap2_v4l2_ctrl_mdin_key_t* pParams);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_V4L2_IOCTL_H__
