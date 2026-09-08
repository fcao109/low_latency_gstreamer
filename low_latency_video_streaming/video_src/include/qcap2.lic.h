#ifndef __QCAP2_LIC_H__
#define __QCAP2_LIC_H__

#include <stdint.h>

#define QCAP2_DECL_LIC(__lic) volatile uint32_t __lic
#define QCAP2_LIC_SUCCEEDED(__lic) (! ((__lic) & 0xCAFECAFE))

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void qcap2_lic_register(volatile uint32_t* pLic);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_LIC_H__