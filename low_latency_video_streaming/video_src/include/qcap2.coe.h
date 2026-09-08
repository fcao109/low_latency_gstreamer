#ifndef __QCAP2_COE_H__
#define __QCAP2_COE_H__

#include "qcap2.h"

#include "NvSIPLClient.hpp"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void qcap2_video_source_set_config_file(qcap2_video_source_t* pThis, const char* strConfigFile);
void qcap2_video_source_set_verbosity(qcap2_video_source_t* pThis, int nVerbosity);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_COE_H__