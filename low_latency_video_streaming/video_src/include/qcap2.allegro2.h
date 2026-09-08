#ifndef __QCAP2_ALLEGRO2_H__
#define __QCAP2_ALLEGRO2_H__

#include "qcap2.h"

extern "C"
{
#include <lib_common/SliceConsts.h>
#include <lib_decode/DecSettings.h>
}

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// qcap2_video_encoder_t
void qcap2_video_encoder_set_filler_ctrl_mode(qcap2_video_encoder_t* pThis, AL_EFillerCtrlMode nFillerCtrlMode);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_ALLEGRO2_H__