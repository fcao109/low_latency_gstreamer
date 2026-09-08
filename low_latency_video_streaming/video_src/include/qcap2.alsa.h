#ifndef __QCAP2_ALSA_H__
#define __QCAP2_ALSA_H__

#include "qcap2.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// qcap2_audio_source_t
void qcap2_audio_source_set_alsa_card(qcap2_audio_source_t* pThis, int nCard);
void qcap2_audio_source_set_alsa_device(qcap2_audio_source_t* pThis, int nDevice);

// qcap2_audio_sink_t
void qcap2_audio_sink_set_alsa_card(qcap2_audio_sink_t* pThis, int nCard);
void qcap2_audio_sink_set_alsa_device(qcap2_audio_sink_t* pThis, int nDevice);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_ALSA_H__
