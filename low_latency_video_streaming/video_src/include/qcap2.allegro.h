#ifndef __QCAP2_ALLEGRO_H__
#define __QCAP2_ALLEGRO_H__

#include "qcap2.h"

extern "C"
{
#include <lib_common/SliceConsts.h>
#include <lib_decode/lib_decode.h>
}

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// qcap2_video_encoder_t
void qcap2_video_encoder_set_filler_ctrl_mode(qcap2_video_encoder_t* pThis, AL_EFillerCtrlMode nFillerCtrlMode);

// qcap2_video_decoder_t
void qcap2_video_decoder_set_input_mode(qcap2_video_decoder_t* pThis, AL_EDecInputMode nInputMode);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_ALLEGRO_H__