#ifndef __QCAP2_DRM_H__
#define __QCAP2_DRM_H__

#include "qcap2.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// utilities
int qcap2_get_drm_fd();
void qcap2_put_drm_fd(int fd);

// qcap2_video_sink_t
void qcap2_video_sink_set_connector_id(qcap2_video_sink_t* pThis, uint32_t nConnectorId);
void qcap2_video_sink_set_crtc_id(qcap2_video_sink_t* pThis, uint32_t nCrtcId);
void qcap2_video_sink_set_plane_id(qcap2_video_sink_t* pThis, uint32_t nPlaneId);
void qcap2_video_sink_set_drm_rect(qcap2_video_sink_t* pThis, int x, int y, int width, int height);
void qcap2_video_sink_set_drm_zpos(qcap2_video_sink_t* pThis, int zpos);
void qcap2_video_sink_set_drm_alpha(qcap2_video_sink_t* pThis, int alpha);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_DRM_H__