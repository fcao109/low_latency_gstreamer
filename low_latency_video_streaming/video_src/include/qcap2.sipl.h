#ifndef __QCAP2_SIPL_H__
#define __QCAP2_SIPL_H__

#include "qcap2.h"

#include "NvSIPLClient.hpp"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// qcap2_rcbuffer_t
QRESULT qcap2_rcbuffer_get_sipl_buffer(qcap2_rcbuffer_t* pRCBuffer, nvsipl::INvSIPLClient::INvSIPLBuffer** ppSIPLBuffer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_SIPL_H__