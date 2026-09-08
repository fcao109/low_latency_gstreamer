#ifndef __QCAP2_BUFFER_V2_H__
#define __QCAP2_BUFFER_V2_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QCAP2_BUFFER_V2_MAX_PLANES 4

/* Platform & Symbol Export */
#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef QCAP2_BUFFER_V2_EXPORTS
    #define QCAP2_BUFFER_V2_API __declspec(dllexport)
  #else
    #define QCAP2_BUFFER_V2_API __declspec(dllimport)
  #endif
#else
  #if __GNUC__ >= 4
    #define QCAP2_BUFFER_V2_API __attribute__((visibility("default")))
  #else
    #define QCAP2_BUFFER_V2_API
  #endif
#endif

/* Result codes */
typedef int qcap2_status_t;
#define QCAP2_OK                 0
#define QCAP2_ERR_INVALID_PARAM -1
#define QCAP2_ERR_NO_MEMORY     -2
#define QCAP2_ERR_BUSY          -3
#define QCAP2_ERR_TIMEOUT       -4
#define QCAP2_ERR_STATE         -5

/* Memory Domains */
typedef enum qcap2_memory_domain_t {
    QCAP2_MEMORY_DOMAIN_UNKNOWN       = 0,
    QCAP2_MEMORY_DOMAIN_CPU           = 1,
    QCAP2_MEMORY_DOMAIN_DMA_BUF       = 2,
    QCAP2_MEMORY_DOMAIN_DRM           = 3,
    QCAP2_MEMORY_DOMAIN_CUDA_DEVICE   = 4,
    QCAP2_MEMORY_DOMAIN_CUDA_HOST     = 5,
    QCAP2_MEMORY_DOMAIN_CUDA_MANAGED  = 6,
    QCAP2_MEMORY_DOMAIN_NVBUF         = 7
} qcap2_memory_domain_t;

/* Payload Types */
typedef enum qcap2_buffer_payload_type_t {
    QCAP2_PAYLOAD_TYPE_UNKNOWN    = 0,
    QCAP2_PAYLOAD_TYPE_RAW_VIDEO  = 1,
    QCAP2_PAYLOAD_TYPE_RAW_AUDIO  = 2,
    QCAP2_PAYLOAD_TYPE_PACKET     = 3,
    QCAP2_PAYLOAD_TYPE_OPAQUE     = 4
} qcap2_buffer_payload_type_t;

/* Multi-plane Descriptor */
typedef struct qcap2_plane_descriptor_t {
    int         fd;          /* DMA-BUF fd / DRM Prime fd (-1 if N/A) */
    uint32_t    handle;      /* DRM GEM handle (0 if N/A) */
    void*       pVirAddr;    /* CPU Virtual Address (NULL if unmapped) */
    uintptr_t   nPhyAddr;    /* Physical / Device Pointer */
    size_t      offset;      /* Byte offset into plane memory */
    size_t      pitch;       /* Stride in bytes */
    size_t      size;        /* Plane byte size */
} qcap2_plane_descriptor_t;

/* Domain Sub-Descriptors */
typedef struct qcap2_dmabuf_desc_t {
    int         fds[QCAP2_BUFFER_V2_MAX_PLANES];
    size_t      offsets[QCAP2_BUFFER_V2_MAX_PLANES];
    size_t      pitches[QCAP2_BUFFER_V2_MAX_PLANES];
    uint64_t    modifier;
    int         sync_fence_fd;
} qcap2_dmabuf_desc_t;

typedef struct qcap2_drmbuf_desc_t {
    int         drm_fd;
    uint32_t    fb_id;
    uint32_t    handles[QCAP2_BUFFER_V2_MAX_PLANES];
    uint32_t    pitches[QCAP2_BUFFER_V2_MAX_PLANES];
    uint32_t    offsets[QCAP2_BUFFER_V2_MAX_PLANES];
    uint32_t    format;
    uint32_t    width;
    uint32_t    height;
    uint64_t    modifier;
} qcap2_drmbuf_desc_t;

typedef struct qcap2_cuda_desc_t {
    void*       pCudaPtrs[QCAP2_BUFFER_V2_MAX_PLANES];
    size_t      pitches[QCAP2_BUFFER_V2_MAX_PLANES];
    void*       cu_graphics_resource;
    void*       egl_frame;
    int         cuda_device_id;
} qcap2_cuda_desc_t;

/* Top-level Descriptor */
typedef struct qcap2_buffer_descriptor_t {
    uint32_t                    version;        /* Descriptor version (1) */
    qcap2_buffer_payload_type_t payload_type;
    qcap2_memory_domain_t       domain;

    uint32_t                    pixel_format;   /* Format FOURCC or ID */
    uint32_t                    width;
    uint32_t                    height;
    uint32_t                    sample_rate;
    uint32_t                    channels;

    uint32_t                    plane_count;
    qcap2_plane_descriptor_t    planes[QCAP2_BUFFER_V2_MAX_PLANES];

    uint64_t                    pts;
    uint64_t                    dts;
    uint64_t                    sequence;

    union {
        qcap2_dmabuf_desc_t     dmabuf;
        qcap2_drmbuf_desc_t     drmbuf;
        qcap2_cuda_desc_t       cuda;
    } domain_desc;
} qcap2_buffer_descriptor_t;

/* Opaque Buffer Handle */
struct qcap2_buffer_v2;
typedef struct qcap2_buffer_v2 qcap2_buffer_v2_t;

/* Single Free Delegate (FFmpeg/GStreamer paradigm):
 * Invoked when refcount drops to 0.
 * Return true if the callback handled recycling (do not delete buffer struct).
 * Return false to allow release() to free default plane memory and delete the buffer struct.
 */
typedef bool (*qcap2_buffer_free_fn)(qcap2_buffer_v2_t* pBuffer, void* pUserData);

/* Buffer Operations */
QCAP2_BUFFER_V2_API qcap2_buffer_v2_t* qcap2_buffer_v2_create(
    const qcap2_buffer_descriptor_t* pDesc,
    qcap2_buffer_free_fn             pFreeFn,
    void*                            pUserData
);

QCAP2_BUFFER_V2_API void qcap2_buffer_v2_retain(qcap2_buffer_v2_t* pBuffer);
QCAP2_BUFFER_V2_API void qcap2_buffer_v2_release(qcap2_buffer_v2_t* pBuffer);

QCAP2_BUFFER_V2_API void* qcap2_buffer_v2_pin_data(qcap2_buffer_v2_t* pBuffer);
QCAP2_BUFFER_V2_API void  qcap2_buffer_v2_unpin_data(qcap2_buffer_v2_t* pBuffer);

QCAP2_BUFFER_V2_API qcap2_status_t qcap2_buffer_v2_get_descriptor(
    const qcap2_buffer_v2_t*   pBuffer,
    qcap2_buffer_descriptor_t* pOutDesc
);

QCAP2_BUFFER_V2_API int qcap2_buffer_v2_refcount(const qcap2_buffer_v2_t* pBuffer);
QCAP2_BUFFER_V2_API int qcap2_buffer_v2_pincount(const qcap2_buffer_v2_t* pBuffer);

/* Opaque Buffer Pool Handle */
struct qcap2_buffer_pool_v2;
typedef struct qcap2_buffer_pool_v2 qcap2_buffer_pool_v2_t;

/* Buffer Pool Operations */
QCAP2_BUFFER_V2_API qcap2_buffer_pool_v2_t* qcap2_buffer_pool_v2_create(
    const qcap2_buffer_descriptor_t* pTemplateDesc,
    uint32_t                         nCapacity
);

QCAP2_BUFFER_V2_API qcap2_status_t qcap2_buffer_pool_v2_acquire(
    qcap2_buffer_pool_v2_t*  pPool,
    qcap2_buffer_v2_t**      ppBuffer,
    uint32_t                 nTimeoutMs
);

QCAP2_BUFFER_V2_API void qcap2_buffer_pool_v2_stop(qcap2_buffer_pool_v2_t* pPool);
QCAP2_BUFFER_V2_API void qcap2_buffer_pool_v2_destroy(qcap2_buffer_pool_v2_t* pPool);

QCAP2_BUFFER_V2_API uint32_t qcap2_buffer_pool_v2_get_capacity(const qcap2_buffer_pool_v2_t* pPool);
QCAP2_BUFFER_V2_API uint32_t qcap2_buffer_pool_v2_get_available(const qcap2_buffer_pool_v2_t* pPool);

#ifdef __cplusplus
}
#endif

#endif /* __QCAP2_BUFFER_V2_H__ */
