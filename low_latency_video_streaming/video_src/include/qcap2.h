#ifndef __QCAP2_H__
#define __QCAP2_H__

#include "qcap.common.h"

#include <stdint.h>
#include <stddef.h>

/*
 * Recovers a containing object from a pointer to one of its members.
 *
 * `ptr` must be non-NULL and point exactly to `member` within a live `type`
 * object. In C++, `type` must be a standard-layout type. The macro performs no
 * validation; a mismatched pointer, type, or member results in undefined
 * behavior. This macro uses GNU statement-expression syntax and therefore
 * requires compiler support for that extension.
 */
#define qcap2_container_of(ptr, type, member) ({ \
	void* __mptr = (void*)(ptr); \
	((type *)((char *)__mptr - offsetof(type, member))); \
})

struct qcap2_av_frame_t;
struct qcap2_av_packet_t;
struct qcap2_input_format_t;
struct qcap2_video_format_t;
struct qcap2_audio_format_t;
struct qcap2_video_encoder_property_t;
struct qcap2_audio_encoder_property_t;
struct qcap2_video_encoder_dynamic_property_t;
struct qcap2_media_info_t;
struct qcap2_program_info_t;
struct qcap2_benaphore_lock_t;

struct qcap2_video_scaler_t;
struct qcap2_audio_resampler_t;
struct qcap2_video_encoder_t;
struct qcap2_video_decoder_t;
struct qcap2_audio_encoder_t;
struct qcap2_audio_decoder_t;
struct qcap2_frame_pool_t;
struct qcap2_packet_pool_t;
struct qcap2_event_t;
struct qcap2_event_handlers_t;
/*
 * Opaque buffer handle. Manage it only with the qcap2_rcbuffer_* APIs and do
 * not treat new/delete and add_ref/release as interchangeable operation pairs.
 */
struct qcap2_rcbuffer_t;
struct qcap2_rcbuffer_queue_t;
struct qcap2_timer_t;
struct qcap2_window_t;
struct qcap2_block_lock_t;
struct qcap2_binder_t;

struct qcap2_qdev_enum_t;
struct qcap2_qdev_info_t;
struct qcap2_qdev_t;
struct qcap2_video_source_t;
struct qcap2_video_sink_t;
struct qcap2_audio_source_t;
struct qcap2_audio_sink_t;
struct qcap2_demuxer_t;
struct qcap2_muxer_t;
struct qcap2_video_matte_t;
struct qcap2_video_blender_t;
struct qcap2_bitstream_filter_t;
struct qcap2_dns_source_t;
struct qcap2_clock_source_t;

// graphics
struct qcap2_font_atlas_t;
struct qcap2_graphics_t;

struct qcap2_build_config_t {
	int major, minor, patch;
	int qcap_major, qcap_minor, qcap_patch;
	const char* build_date;
	const char* build_time;
	const char* branch;
	const char* commit;
	const char* mods;
};

/*
 * Compatibility storage for a media frame. Initialize it with
 * qcap2_av_frame_init(), use only qcap2_av_frame_* accessors, and release any
 * allocated buffer with the matching API. Do not inspect padding, cast it to
 * another type, or copy it as raw bytes.
 */
struct qcap2_av_frame_t {
	uint8_t padding[576];
};

/*
 * Compatibility storage for a media packet. Initialize it with
 * qcap2_av_packet_init(), use only qcap2_av_packet_* accessors, and release any
 * allocated buffer with the matching API. Do not inspect padding, cast it to
 * another type, or copy it as raw bytes.
 */
struct qcap2_av_packet_t {
	uint8_t padding[128];
};

struct qcap2_dns_event_t {
	int error;
	uint8_t sockaddr[32];
	char name[256];
};

struct qcap2_clock_event_t {
	int error;
	int64_t diff;
};

struct qcap2_rational_t {
	int num;
	int den;
};

// enum values
enum qcap2_test_pattern_type_t {
	QCAP2_TEST_PATTERN_0,
	QCAP2_TEST_PATTERN_1,
	QCAP2_TEST_PATTERN_SMPTE_75_COLOR_BARS,

	QCAP2_TEST_PATTERN_RED = 256, // Y: [16,235], CbCr: [16,240]
	QCAP2_TEST_PATTERN_GREEN,
	QCAP2_TEST_PATTERN_BLUE,
	QCAP2_TEST_PATTERN_WHITE,
	QCAP2_TEST_PATTERN_BLACK,
};

enum qcap2_frame_pool_backend_type_t {
	QCAP2_FRAME_POOL_BACKEND_TYPE_UNKNOWN,

	QCAP2_FRAME_POOL_BACKEND_TYPE_DEFAULT,
	QCAP2_FRAME_POOL_BACKEND_TYPE_RKMPP,
	QCAP2_FRAME_POOL_BACKEND_TYPE_QDMABUF,
};

enum qcap2_video_scaler_backend_type_t {
	QCAP2_VIDEO_SCALER_BACKEND_TYPE_UNKNOWN,

	QCAP2_VIDEO_SCALER_BACKEND_TYPE_DEFAULT,
	QCAP2_VIDEO_SCALER_BACKEND_TYPE_NPP,
	QCAP2_VIDEO_SCALER_BACKEND_TYPE_LBL_COPY,
	QCAP2_VIDEO_SCALER_BACKEND_TYPE_NVT_HDAL,
	QCAP2_VIDEO_SCALER_BACKEND_TYPE_FF_FILTER_GRAPH,

	QCAP2_VIDEO_SCALER_BACKEND_TYPE_EXPERIMENTAL = 9999,
};

enum qcap2_qdev_type_t {
	QCAP2_QDEV_TYPE_UNKNOWN,

	QCAP2_QDEV_TYPE_UB3300,
	QCAP2_QDEV_TYPE_MZ0380,
	QCAP2_QDEV_TYPE_SC440N2_GMSL,
	QCAP2_QDEV_TYPE_CV0830,
};

enum qcap2_window_backend_type_t {
	QCAP2_WINDOW_BACKEND_TYPE_UNKNOWN,

	QCAP2_WINDOW_BACKEND_TYPE_NULL,
	QCAP2_WINDOW_BACKEND_TYPE_FAKE,
	QCAP2_WINDOW_BACKEND_TYPE_X11,
};

enum qcap2_video_source_backend_type_t {
	QCAP2_VIDEO_SOURCE_BACKEND_TYPE_UNKNOWN,

	QCAP2_VIDEO_SOURCE_BACKEND_TYPE_USER,
	QCAP2_VIDEO_SOURCE_BACKEND_TYPE_V4L2,
	QCAP2_VIDEO_SOURCE_BACKEND_TYPE_PYLON,
	QCAP2_VIDEO_SOURCE_BACKEND_TYPE_VITIS,
	QCAP2_VIDEO_SOURCE_BACKEND_TYPE_XLNX,
	QCAP2_VIDEO_SOURCE_BACKEND_TYPE_V4L2_SG,
	QCAP2_VIDEO_SOURCE_BACKEND_TYPE_LBLWR,
	QCAP2_VIDEO_SOURCE_BACKEND_TYPE_NVT_HDAL,
	QCAP2_VIDEO_SOURCE_BACKEND_TYPE_HSB,
	QCAP2_VIDEO_SOURCE_BACKEND_TYPE_TPG,
	QCAP2_VIDEO_SOURCE_BACKEND_TYPE_LT6911,
	QCAP2_VIDEO_SOURCE_BACKEND_TYPE_IMX585,
	QCAP2_VIDEO_SOURCE_BACKEND_TYPE_COE,
};

enum qcap2_video_sink_backend_type_t {
	QCAP2_VIDEO_SINK_BACKEND_TYPE_UNKNOWN,

	QCAP2_VIDEO_SINK_BACKEND_TYPE_DAVMF,
	QCAP2_VIDEO_SINK_BACKEND_TYPE_GSTREAMER,
	QCAP2_VIDEO_SINK_BACKEND_TYPE_VITIS,
	QCAP2_VIDEO_SINK_BACKEND_TYPE_V4L2CAP,
	QCAP2_VIDEO_SINK_BACKEND_TYPE_XLNX,
	QCAP2_VIDEO_SINK_BACKEND_TYPE_L4T,
	QCAP2_VIDEO_SINK_BACKEND_TYPE_LBLRD,
	QCAP2_VIDEO_SINK_BACKEND_TYPE_NVT_HDAL,
	QCAP2_VIDEO_SINK_BACKEND_TYPE_V4L2,
	QCAP2_VIDEO_SINK_BACKEND_TYPE_XLNX2,

	QCAP2_VIDEO_SINK_BACKEND_TYPE_EXPERIMENTAL = 9999,
};

enum qcap2_audio_test_pattern_type_t {
	QCAP2_AUDIO_TEST_PATTERN_SILENCE,
	QCAP2_AUDIO_TEST_PATTERN_SINE_1KHZ_MINUS_20DBFS,
};

enum qcap2_audio_source_backend_type_t {
	QCAP2_AUDIO_SOURCE_BACKEND_TYPE_UNKNOWN,

	QCAP2_AUDIO_SOURCE_BACKEND_TYPE_ALSA,
	QCAP2_AUDIO_SOURCE_BACKEND_TYPE_VITIS,
	QCAP2_AUDIO_SOURCE_BACKEND_TYPE_V4L2,
	QCAP2_AUDIO_SOURCE_BACKEND_TYPE_NVT_HDAL,
	QCAP2_AUDIO_SOURCE_BACKEND_TYPE_TPG,
	QCAP2_AUDIO_SOURCE_BACKEND_TYPE_FFMPEG,
};

enum qcap2_audio_sink_backend_type_t {
	QCAP2_AUDIO_SINK_BACKEND_TYPE_UNKNOWN,

	QCAP2_AUDIO_SINK_BACKEND_TYPE_ALSA,
	QCAP2_AUDIO_SINK_BACKEND_TYPE_VITIS,
	QCAP2_AUDIO_SINK_BACKEND_TYPE_V4L2CAP,
	QCAP2_AUDIO_SINK_BACKEND_TYPE_NVT_HDAL,
};

enum qcap2_demuxer_type_t {
	QCAP2_DEMUXER_TYPE_UNKNOWN,

	QCAP2_DEMUXER_TYPE_DEFAULT,
	QCAP2_DEMUXER_TYPE_PYLON,
	QCAP2_DEMUXER_TYPE_USBCAM,
	QCAP2_DEMUXER_TYPE_FIFO,
	QCAP2_DEMUXER_TYPE_RTP,
	QCAP2_DEMUXER_TYPE_RTSP,
	QCAP2_DEMUXER_TYPE_JSRTSP,
	QCAP2_DEMUXER_TYPE_VITIS,
	QCAP2_DEMUXER_TYPE_YUANCAP,
	QCAP2_DEMUXER_TYPE_NVT_HDAL,
	QCAP2_DEMUXER_TYPE_SC6F0,

	QCAP2_DEMUXER_TYPE_EXPERIMENTAL = 9999,
};

enum qcap2_muxer_type_t {
	QCAP2_MUXER_TYPE_UNKNOWN,

	QCAP2_MUXER_TYPE_DEFAULT,
	QCAP2_MUXER_TYPE_JSRTSP,
	QCAP2_MUXER_TYPE_RTSP,
	QCAP2_MUXER_TYPE_SC6F0, // TODO
	QCAP2_MUXER_TYPE_FILE,
	QCAP2_MUXER_TYPE_HLS,

	QCAP2_MUXER_TYPE_EXPERIMENTAL = 9999,
};

enum qcap2_video_matte_backend_type_t {
	QCAP2_VIDEO_MATTE_BACKEND_TYPE_UNKNOWN,

	QCAP2_VIDEO_MATTE_BACKEND_TYPE_OPMATTING,
};

enum qcap2_video_blender_backend_type_t {
	QCAP2_VIDEO_BLENDER_BACKEND_TYPE_UNKNOWN,

	QCAP2_VIDEO_BLENDER_BACKEND_TYPE_CUDA,
};

enum qcap2_color_space_t {
	QCAP2_COLOR_SPACE_UNKNOWN,

	QCAP2_COLOR_SPACE_JPEG, // YCbCr: [0,255]
	QCAP2_COLOR_SPACE_BT601, // Y: [16,235], CbCr: [16,240]
	QCAP2_COLOR_SPACE_BT709, // Y: [16,235], CbCr: [16,240]
	QCAP2_COLOR_SPACE_BT601_FULL, // YCbCr: [0,255]
	QCAP2_COLOR_SPACE_BT709_FULL, // YCbCr: [0,255]
	QCAP2_COLOR_SPACE_BT2020, // Y: [16,235], CbCr: [16,240]
	QCAP2_COLOR_SPACE_BT2020_FULL, // YCbCr: [0,255]
};

enum qcap2_color_range_t {
	QCAP2_COLOR_RANGE_UNKNOWN,

	QCAP2_COLOR_RANGE_LIMITED, // [16,235]
	QCAP2_COLOR_RANGE_FULL, // [0,255]
};

enum qcap2_stereoscopic_type_t {
	QCAP2_STEREOSCOPIC_TYPE_UNKNOWN,

	QCAP2_STEREOSCOPIC_TYPE_LINE_BY_LINE,
	QCAP2_STEREOSCOPIC_TYPE_TOP_BOTTOM,
	QCAP2_STEREOSCOPIC_TYPE_LEFT_RIGHT,
};

enum qcap2_bitstream_filter_backend_type_t {
	QCAP2_BITSTREAM_FILTER_BACKEND_TYPE_UNKNOWN,

	QCAP2_BITSTREAM_FILTER_BACKEND_TYPE_NOOP,
	QCAP2_BITSTREAM_FILTER_BACKEND_TYPE_DATA_APPEND,
	QCAP2_BITSTREAM_FILTER_BACKEND_TYPE_DATA_EXTRACT,
	QCAP2_BITSTREAM_FILTER_BACKEND_TYPE_DATA_REMOVE,
};

enum qcap2_field_type_t {
	QCAP2_FIELD_NONE,
	QCAP2_FIELD_TOP, // odd
	QCAP2_FIELD_BOTTOM, // even
	QCAP2_FIELD_INTERLACED,
};

enum qcap2_buffer_hint_t {
	QCAP2_BUFFER_HINT_DEFAULT,

	QCAP2_BUFFER_HINT_CUDA,
	QCAP2_BUFFER_HINT_CUDAHOST,
	QCAP2_BUFFER_HINT_CUDA_MANAGED,

	QCAP2_BUFFER_HINT_CUDA_HOST = QCAP2_BUFFER_HINT_CUDAHOST,
};

enum qcap2_graphics_backend_type_t {
	QCAP2_GRAPHICS_BACKEND_TYPE_UNKNOWN,

	QCAP2_GRAPHICS_BACKEND_TYPE_DEFAULT,

	QCAP2_GRAPHICS_BACKEND_TYPE_EXPERIMENTAL = 9999,
};

enum qcap2_snapshot_format_t {
	QCAP2_SNAPSHOT_FORMAT_BMP = 0,
	QCAP2_SNAPSHOT_FORMAT_JPG = 1,
	QCAP2_SNAPSHOT_FORMAT_PNG = 2,
};

// callbacks
/*
 * Resource-return/recycle notification for producer-supplied media storage.
 *
 * The callback receives the same borrowed pData pointer passed to
 * qcap2_rcbuffer_new(). It may release or recycle media resources referenced by
 * that storage, but must not destroy the pData storage or an enclosing object
 * while the buffer handle still exists. The callback is not guaranteed to run
 * exactly once, may run again when a buffer is reused, and is not guaranteed to
 * run merely because the wrapper is deleted.
 */
typedef void (*qcap2_on_free_resource_t)(PVOID pData);
typedef QRETURN (*qcap2_on_event_t)(PVOID pUserData);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// utilities
int64_t qcap2_get_time();
void qcap2_get_build_config(qcap2_build_config_t* pBuildConfig);
QRESULT qcap2_save_raw_video_frame(qcap2_rcbuffer_t* pRCBuffer, const char* prefix);
QRESULT qcap2_print_video_frame_info(qcap2_rcbuffer_t* pRCBuffer, const char* prefix);
QRESULT qcap2_print_audio_sample_info(qcap2_rcbuffer_t* pRCBuffer, const char* prefix);
QRESULT qcap2_print_packet_info(qcap2_rcbuffer_t* pRCBuffer, const char* prefix);
QRESULT qcap2_fill_video_test_pattern(qcap2_rcbuffer_t* pRCBuffer, int nType); // qcap2_test_pattern_type_t
QRESULT qcap2_load_picture(qcap2_rcbuffer_t* pRCBuffer, const char* strFilePath);
QRESULT qcap2_get_picture_info(const char* strFilePath, qcap2_video_format_t* pVideoFormat);
QRESULT qcap2_save_picture(qcap2_rcbuffer_t* pRCBuffer, const char* strFilePath, int nQuality);
QRESULT qcap2_snapshot_to_buffer(qcap2_rcbuffer_t* pRCBuffer, int nFormat, int nQuality, uint8_t* pDstBuffer, size_t* pDstBufferSize);
const char* qcap2_get_pix_fmt_name(int nFormat);
const char* qcap2_get_sample_fmt_name(int nFormat);
qcap2_rational_t qcap2_d2q(double d, int nMax);

// debug
int qcap2_debug_get(int n);
void qcap2_debug_set(int n, int v);
int qcap2_debug_fetch_add(int n, int v);
int qcap2_debug_fetch_sub(int n, int v);

// qcap2_rcbuffer_t

/*
 * @brief Creates a buffer handle around caller-owned media storage.
 * @param[in] pData Borrowed pointer to media storage (usually
 *                  qcap2_av_frame_t or qcap2_av_packet_t). The pointed-to
 *                  storage is not copied and ownership is not transferred. The
 *                  storage and any enclosing object recovered with
 *                  qcap2_container_of() must remain valid until
 *                  qcap2_rcbuffer_delete() returns.
 * @param[in] pOnFreeResource Optional resource-return/recycle notification. It
 *                            receives the same pData pointer and is not an
 *                            exactly-once final destructor callback.
 * @return A creator-owned handle that must eventually be passed to
 *         qcap2_rcbuffer_delete().
 */
qcap2_rcbuffer_t* qcap2_rcbuffer_new(PVOID pData, qcap2_on_free_resource_t pOnFreeResource);

/*
 * @brief Releases the creator ownership returned by qcap2_rcbuffer_new().
 * @param[in] pRCBuffer The refcounted buffer pointer.
 * @note Call exactly once after all add_ref/release pairs and all successful
 *       lock_data/unlock_data pairs are balanced. Do not use this function in
 *       place of qcap2_rcbuffer_release().
 */
void qcap2_rcbuffer_delete(qcap2_rcbuffer_t* pRCBuffer);

/*
 * @brief Exports a typed handle as a legacy (BYTE*, ULONG) tagged pair.
 * @param[in] pRCBuffer The refcounted buffer pointer.
 * @param[out] ppBuffer Receives the legacy buffer pointer.
 * @param[out] pBufferSize Receives the reserved tag value 0xFFFFCAFE.
 * @note This function does not acquire or transfer ownership. Keep the typed
 *       handle valid for as long as the legacy pair may be used.
 */
void qcap2_rcbuffer_to_buffer(qcap2_rcbuffer_t* pRCBuffer, BYTE** ppBuffer, ULONG* pBufferSize);

/*
 * @brief Converts a legacy tagged pair to a typed buffer handle.
 * @param[in] pBuffer The legacy buffer pointer.
 * @param[in] nBufferLen Must be the reserved tag value 0xFFFFCAFE.
 * @return The typed handle, or NULL for a null pointer or wrong tag.
 * @warning A matching tag does not validate pointer origin or lifetime, and
 *          this function does not acquire ownership.
 */
qcap2_rcbuffer_t* qcap2_rcbuffer_cast(BYTE * pBuffer, ULONG nBufferLen);

/*
 * @brief Acquires a shared buffer reference.
 * @param[in] pRCBuffer The refcounted buffer pointer.
 * @note Every acquired reference must be released with
 *       qcap2_rcbuffer_release(). This does not grant data access; use
 *       lock_data/unlock_data while dereferencing the media data.
 */
void qcap2_rcbuffer_add_ref(qcap2_rcbuffer_t* pRCBuffer);

/*
 * @brief Releases a shared buffer reference.
 * @param[in] pRCBuffer The refcounted buffer pointer.
 * @note Use for a reference acquired with add_ref() or transferred by a
 *       producer/pool. Do not use it to release the creator ownership returned
 *       by qcap2_rcbuffer_new(); use qcap2_rcbuffer_delete() for that owner.
 */
void qcap2_rcbuffer_release(qcap2_rcbuffer_t* pRCBuffer);

/*
 * @brief Pins the wrapped data lifetime and returns the media-data pointer.
 * @param[in] pRCBuffer The refcounted buffer pointer.
 * @return Pointer to the pinned media data, or NULL if it is no longer
 *         available.
 * @note Despite the name, this is not a mutex and provides no read/write
 *       exclusion. Every successful call must be balanced by unlock_data.
 */
PVOID qcap2_rcbuffer_lock_data(qcap2_rcbuffer_t* pRCBuffer);

/*
 * @brief Releases one data-lifetime pin.
 * @param[in] pRCBuffer The refcounted buffer pointer.
 * @note Stop using the data pointer before this call.
 */
void qcap2_rcbuffer_unlock_data(qcap2_rcbuffer_t* pRCBuffer);

/*
 * @brief Returns the wrapped data without acquiring a lifetime pin.
 * @param[in] pRCBuffer The refcounted buffer pointer.
 * @return Pointer to the wrapped media data.
 * @warning The caller must already own a lifetime that keeps both wrapper and
 *          data alive. Prefer a balanced lock_data/unlock_data pair.
 */
PVOID qcap2_rcbuffer_get_data(qcap2_rcbuffer_t* pRCBuffer); // risky accessor

/*
 * @brief Returns a legacy data-lifetime diagnostic count.
 * @param[in] pRCBuffer The refcounted buffer pointer.
 * @note Do not use this value to decide whether the caller owns the handle or
 *       whether concurrent data access is safe.
 */
int32_t qcap2_rcbuffer_use_count(qcap2_rcbuffer_t* pRCBuffer);

/*
 * @brief Returns a legacy wrapper-lifetime diagnostic count.
 * @param[in] pRCBuffer The refcounted buffer pointer.
 * @note This is not guaranteed to equal the number of caller-owned references;
 *       do not use it to make ownership or deletion decisions.
 */
int32_t qcap2_rcbuffer_weak_count(qcap2_rcbuffer_t* pRCBuffer);

/*
 * @brief Returns a legacy resource-lifetime diagnostic count.
 * @param[in] pRCBuffer A handle produced by a qcap2 resource or pool API.
 * @note Do not call this for an arbitrary legacy-cast handle, and do not use the
 *       value to make ownership, recycling, or deletion decisions.
 */
int32_t qcap2_rcbuffer_res_count(qcap2_rcbuffer_t* pRCBuffer);

// qcap2_av_frame_t
void qcap2_av_frame_init(qcap2_av_frame_t* pFrame);
void qcap2_av_frame_set_video_property(qcap2_av_frame_t* pFrame, ULONG nColorSpaceType, ULONG nWidth, ULONG nHeight);
void qcap2_av_frame_get_video_property(qcap2_av_frame_t* pFrame, ULONG* pColorSpaceType, ULONG* pWidth, ULONG* pHeight);
void qcap2_av_frame_set_audio_property(qcap2_av_frame_t* pFrame, ULONG nChannels, ULONG nSampleFmt, ULONG nSampleFrequency, ULONG nFrameSize);
void qcap2_av_frame_get_audio_property(qcap2_av_frame_t* pFrame, ULONG* pChannels, ULONG* pSampleFmt, ULONG* pSampleFrequency, ULONG* pFrameSize);
void qcap2_av_frame_set_field_type(qcap2_av_frame_t* pFrame, int nFieldType); // refer to qcap2_field_type_t
void qcap2_av_frame_get_field_type(qcap2_av_frame_t* pFrame, int* pFieldType); // refer to qcap2_field_type_t
void qcap2_av_frame_set_sample_time(qcap2_av_frame_t* pFrame, double dSampleTime);
void qcap2_av_frame_get_sample_time(qcap2_av_frame_t* pFrame, double* pSampleTime);
void qcap2_av_frame_set_pts(qcap2_av_frame_t* pFrame, int64_t nPTS);
void qcap2_av_frame_get_pts(qcap2_av_frame_t* pFrame, int64_t* pPTS);
void qcap2_av_frame_set_pkt_pos(qcap2_av_frame_t* pFrame, int64_t nPktPos);
void qcap2_av_frame_get_pkt_pos(qcap2_av_frame_t* pFrame, int64_t* pPktPos);
void qcap2_av_frame_get_video_bits(qcap2_av_frame_t* pFrame, int64_t* pBits);
void qcap2_av_frame_get_audio_bits(qcap2_av_frame_t* pFrame, int64_t* pBits);
void qcap2_av_frame_set_buffer(qcap2_av_frame_t* pFrame, uint8_t* pBuffer, int nStride);
void qcap2_av_frame_get_buffer(qcap2_av_frame_t* pFrame, uint8_t** ppBuffer, int* pStride);
void qcap2_av_frame_set_buffer1(qcap2_av_frame_t* pFrame, uint8_t* pBuffer[4], int pStride[4]);
void qcap2_av_frame_get_buffer1(qcap2_av_frame_t* pFrame, uint8_t* pBuffer[4], int pStride[4]);
bool qcap2_av_frame_alloc_buffer(qcap2_av_frame_t* pFrame, int align, int valign);
void qcap2_av_frame_free_buffer(qcap2_av_frame_t* pFrame);
QRESULT qcap2_av_frame_copy(qcap2_av_frame_t* pSrcFrame, qcap2_av_frame_t* pDstFrame);
QRESULT qcap2_av_frame_color_range_expand(qcap2_av_frame_t* pSrcFrame, qcap2_av_frame_t* pDstFrame); // limited-range -> full-range
QRESULT qcap2_av_frame_store_picture(qcap2_av_frame_t* pFrame, const char* strFilePath);
QRESULT qcap2_av_frame_store_picture2(qcap2_av_frame_t* pFrame, const char* strFilePath, int nQuality);

// qcap2_av_packet_t
void qcap2_av_packet_init(qcap2_av_packet_t* pPacket);
void qcap2_av_packet_set_property(qcap2_av_packet_t* pPacket, int nStreamIndex, BOOL bIsKeyFrame);
void qcap2_av_packet_get_property(qcap2_av_packet_t* pPacket, int* pStreamIndex, BOOL* pIsKeyFrame);
void qcap2_av_packet_set_sample_time(qcap2_av_packet_t* pPacket, double dSampleTime);
void qcap2_av_packet_get_sample_time(qcap2_av_packet_t* pPacket, double* pSampleTime);
void qcap2_av_packet_set_pts(qcap2_av_packet_t* pPacket, int64_t nPTS);
void qcap2_av_packet_get_pts(qcap2_av_packet_t* pPacket, int64_t* pPTS);
void qcap2_av_packet_set_dts(qcap2_av_packet_t* pPacket, int64_t nDTS);
void qcap2_av_packet_get_dts(qcap2_av_packet_t* pPacket, int64_t* pDTS);
void qcap2_av_packet_set_buffer(qcap2_av_packet_t* pPacket, uint8_t* pBuffer, int nSize);
void qcap2_av_packet_get_buffer(qcap2_av_packet_t* pPacket, uint8_t** ppBuffer, int* pSize);
bool qcap2_av_packet_alloc_buffer(qcap2_av_packet_t* pPacket, int nSize);
void qcap2_av_packet_free_buffer(qcap2_av_packet_t* pPacket);

// qcap2_input_format_t
qcap2_input_format_t* qcap2_input_format_new();
void qcap2_input_format_delete(qcap2_input_format_t* pThis);

// qcap2_video_format_t
qcap2_video_format_t* qcap2_video_format_new();
void qcap2_video_format_delete(qcap2_video_format_t* pThis);
void qcap2_video_format_get_property(qcap2_video_format_t* pThis, ULONG * pColorSpaceType, ULONG * pVideoWidth, ULONG * pVideoHeight, BOOL * pVideoIsInterleaved, double * pVideoFrameRate);
void qcap2_video_format_set_property(qcap2_video_format_t* pThis, ULONG nColorSpaceType, ULONG nVideoWidth, ULONG nVideoHeight, BOOL bVideoIsInterleaved, double dVideoFrameRate);
void qcap2_video_format_get_input(qcap2_video_format_t* pThis, ULONG * pInput);
void qcap2_video_format_set_input(qcap2_video_format_t* pThis, ULONG nInput);

// qcap2_audio_format_t
qcap2_audio_format_t* qcap2_audio_format_new();
void qcap2_audio_format_delete(qcap2_audio_format_t* pThis);
void qcap2_audio_format_get_property(qcap2_audio_format_t* pThis, ULONG * pAudioChannels, ULONG * pAudioBitsPerSample, ULONG * pAudioSampleFrequency);
void qcap2_audio_format_set_property(qcap2_audio_format_t* pThis, ULONG   nAudioChannels, ULONG   nAudioBitsPerSample, ULONG   nAudioSampleFrequency);
void qcap2_audio_format_get_input(qcap2_audio_format_t* pThis, ULONG * pInput);
void qcap2_audio_format_set_input(qcap2_audio_format_t* pThis, ULONG nInput);

// qcap2_video_encoder_property_t
qcap2_video_encoder_property_t* qcap2_video_encoder_property_new();
void qcap2_video_encoder_property_delete(qcap2_video_encoder_property_t* pThis);
void qcap2_video_encoder_property_get_property(qcap2_video_encoder_property_t* pThis, ULONG * pEncoderType, ULONG * pEncoderFormat, ULONG * pColorSpaceType, ULONG * pWidth, ULONG * pHeight, double * pFrameRate, ULONG * pRecordMode, ULONG * pQuality, ULONG * pBitRate, ULONG * pGOP, ULONG * pAspectRatioX, ULONG * pAspectRatioY);
void qcap2_video_encoder_property_set_property(qcap2_video_encoder_property_t* pThis, ULONG   nEncoderType, ULONG   nEncoderFormat, ULONG   nColorSpaceType, ULONG   nWidth, ULONG   nHeight, double   dFrameRate, ULONG   nRecordMode, ULONG   nQuality, ULONG   nBitRate, ULONG   nGOP, ULONG   nAspectRatioX, ULONG   nAspectRatioY);
void qcap2_video_encoder_property_get_property1(qcap2_video_encoder_property_t* pThis, UINT* pGpuNum, ULONG * pEncoderType, ULONG * pEncoderFormat, ULONG * pColorSpaceType, ULONG * pWidth, ULONG * pHeight, double * pFrameRate, ULONG * pRecordProfile, ULONG * pRecordLevel, ULONG * pRecordEntropy, ULONG * pRecordComplexity, ULONG * pRecordMode, ULONG * pQuality, ULONG * pBitRate, ULONG * pGOP, ULONG * pBFrames, BOOL * pIsInterleaved, ULONG * pSlices, ULONG * pLayers, ULONG * pSceneCut, BOOL * pMultiThread, BOOL * pMBBRC, BOOL * pExtBRC, ULONG * pMinQP, ULONG * pMaxQP, ULONG * pVBVMaxRate, ULONG * pVBVBufSize, ULONG * pAspectRatioX, ULONG * pAspectRatioY);
void qcap2_video_encoder_property_set_property1(qcap2_video_encoder_property_t* pThis, UINT nGpuNum, ULONG   nEncoderType, ULONG   nEncoderFormat, ULONG   nColorSpaceType, ULONG   nWidth, ULONG   nHeight, double   dFrameRate, ULONG   nRecordProfile , ULONG   nRecordLevel, ULONG   nRecordEntropy, ULONG   nRecordComplexity, ULONG   nRecordMode, ULONG   nQuality, ULONG   nBitRate, ULONG   nGOP, ULONG   nBFrames, BOOL   bIsInterleaved, ULONG   nSlices, ULONG   nLayers, ULONG   nSceneCut, BOOL   bMultiThread, BOOL   bMBBRC, BOOL   bExtBRC, ULONG   nMinQP , ULONG   nMaxQP, ULONG   nVBVMaxRate , ULONG   nVBVBufSize, ULONG   nAspectRatioX, ULONG   nAspectRatioY);
void qcap2_video_encoder_property_get_type(qcap2_video_encoder_property_t* pThis, ULONG * pEncoderType);
void qcap2_video_encoder_property_set_type(qcap2_video_encoder_property_t* pThis, ULONG   nEncoderType);
void qcap2_video_encoder_property_get_format(qcap2_video_encoder_property_t* pThis, ULONG * pEncoderFormat);
void qcap2_video_encoder_property_set_format(qcap2_video_encoder_property_t* pThis, ULONG   nEncoderFormat);
void qcap2_video_encoder_property_get_color_space(qcap2_video_encoder_property_t* pThis, ULONG * pColorSpaceType);
void qcap2_video_encoder_property_set_color_space(qcap2_video_encoder_property_t* pThis, ULONG   nColorSpaceType);
void qcap2_video_encoder_property_get_resolution(qcap2_video_encoder_property_t* pThis, ULONG * pWidth, ULONG * pHeight);
void qcap2_video_encoder_property_set_resolution(qcap2_video_encoder_property_t* pThis, ULONG   nWidth, ULONG   nHeight);
void qcap2_video_encoder_property_get_bitrate(qcap2_video_encoder_property_t* pThis, ULONG * pBitRate);
void qcap2_video_encoder_property_set_bitrate(qcap2_video_encoder_property_t* pThis, ULONG   nBitRate);
void qcap2_video_encoder_property_set_cbr_variation(qcap2_video_encoder_property_t* pThis, ULONG nCBRVariation);
void qcap2_video_encoder_property_get_cbr_variation(qcap2_video_encoder_property_t* pThis, ULONG* pCBRVariation);
void qcap2_video_encoder_property_set_color_range(qcap2_video_encoder_property_t* pThis, ULONG nColorRange);
void qcap2_video_encoder_property_get_color_range(qcap2_video_encoder_property_t* pThis, ULONG* pColorRange);
void qcap2_video_encoder_property_set_source_frame_rate(qcap2_video_encoder_property_t* pThis, double dSourceFrameRate);
void qcap2_video_encoder_property_get_source_frame_rate(qcap2_video_encoder_property_t* pThis, double* pSourceFrameRate);
void qcap2_video_encoder_property_set_rotation(qcap2_video_encoder_property_t* pThis, ULONG nRotation);
void qcap2_video_encoder_property_get_rotation(qcap2_video_encoder_property_t* pThis, ULONG* pRotation);
void qcap2_video_encoder_property_set_ideal_timestamp(qcap2_video_encoder_property_t* pThis, BOOL bIdealTimestamp);
void qcap2_video_encoder_property_get_ideal_timestamp(qcap2_video_encoder_property_t* pThis, BOOL* pIdealTimestamp);
void qcap2_video_encoder_property_set_encoder_osd(qcap2_video_encoder_property_t* pThis, BOOL bEncoderOSD);
void qcap2_video_encoder_property_get_encoder_osd(qcap2_video_encoder_property_t* pThis, BOOL* pEncoderOSD);
void qcap2_video_encoder_property_set_fixed_function(qcap2_video_encoder_property_t* pThis, BOOL bEnableFF);
void qcap2_video_encoder_property_get_fixed_function(qcap2_video_encoder_property_t* pThis, BOOL* pEnableFF);
void qcap2_video_encoder_property_set_low_delay(qcap2_video_encoder_property_t* pThis, BOOL bEnableLowDelay);
void qcap2_video_encoder_property_get_low_delay(qcap2_video_encoder_property_t* pThis, BOOL* pEnableLowDelay);
void qcap2_video_encoder_property_set_rtmp_compat(qcap2_video_encoder_property_t* pThis, BOOL bEnableRTMPCompatible);
void qcap2_video_encoder_property_get_rtmp_compat(qcap2_video_encoder_property_t* pThis, BOOL* pEnableRTMPCompatible);
void qcap2_video_encoder_property_set_time_scale_factor(qcap2_video_encoder_property_t* pThis, ULONG nTimeScaleFactor);
void qcap2_video_encoder_property_get_time_scale_factor(qcap2_video_encoder_property_t* pThis, ULONG* pTimeScaleFactor);
void qcap2_video_encoder_property_set_high_perf(qcap2_video_encoder_property_t* pThis, BOOL bEnableHighPerf);
void qcap2_video_encoder_property_get_high_perf(qcap2_video_encoder_property_t* pThis, BOOL* pEnableHighPerf);

// qcap2_audio_encoder_property_t
qcap2_audio_encoder_property_t* qcap2_audio_encoder_property_new();
void qcap2_audio_encoder_property_delete(qcap2_audio_encoder_property_t* pThis);
void qcap2_audio_encoder_property_get_property(qcap2_audio_encoder_property_t* pThis, ULONG * pEncoderType, ULONG * pEncoderFormat, ULONG * pChannels, ULONG * pBitsPerSample, ULONG * pSampleFrequency);
void qcap2_audio_encoder_property_set_property(qcap2_audio_encoder_property_t* pThis, ULONG   nEncoderType, ULONG   nEncoderFormat, ULONG   nChannels, ULONG   nBitsPerSample, ULONG   nSampleFrequency);
void qcap2_audio_encoder_property_get_property1(qcap2_audio_encoder_property_t* pThis, ULONG * pEncoderType, ULONG * pEncoderFormat, ULONG * pChannels, ULONG * pBitsPerSample, ULONG * pSampleFrequency, ULONG * pBitRate);
void qcap2_audio_encoder_property_set_property1(qcap2_audio_encoder_property_t* pThis, ULONG   nEncoderType, ULONG   nEncoderFormat, ULONG   nChannels, ULONG   nBitsPerSample, ULONG   nSampleFrequency, ULONG   nBitRate);
void qcap2_audio_encoder_property_get_type(qcap2_audio_encoder_property_t* pThis, ULONG * pEncoderType);
void qcap2_audio_encoder_property_set_type(qcap2_audio_encoder_property_t* pThis, ULONG   nEncoderType);
void qcap2_audio_encoder_property_get_format(qcap2_audio_encoder_property_t* pThis, ULONG * pEncoderFormat);
void qcap2_audio_encoder_property_set_format(qcap2_audio_encoder_property_t* pThis, ULONG   nEncoderFormat);
void qcap2_audio_encoder_property_get_channels(qcap2_audio_encoder_property_t* pThis, ULONG * pChannels);
void qcap2_audio_encoder_property_set_channels(qcap2_audio_encoder_property_t* pThis, ULONG   nChannels);
void qcap2_audio_encoder_property_get_bits_per_sample(qcap2_audio_encoder_property_t* pThis, ULONG * pBitsPerSample);
void qcap2_audio_encoder_property_set_bits_per_sample(qcap2_audio_encoder_property_t* pThis, ULONG   nBitsPerSample);
void qcap2_audio_encoder_property_get_sample_frequency(qcap2_audio_encoder_property_t* pThis, ULONG * pSampleFrequency);
void qcap2_audio_encoder_property_set_sample_frequency(qcap2_audio_encoder_property_t* pThis, ULONG   nSampleFrequency);
void qcap2_audio_encoder_property_get_bitrate(qcap2_audio_encoder_property_t* pThis, ULONG * pBitRate);
void qcap2_audio_encoder_property_set_bitrate(qcap2_audio_encoder_property_t* pThis, ULONG   nBitRate);
void qcap2_audio_encoder_property_set_ideal_timestamp(qcap2_audio_encoder_property_t* pThis, BOOL bIdealTimestamp);
void qcap2_audio_encoder_property_get_ideal_timestamp(qcap2_audio_encoder_property_t* pThis, BOOL* pIdealTimestamp);

// qcap2_video_encoder_dynamic_property_t
qcap2_video_encoder_dynamic_property_t* qcap2_video_encoder_dynamic_property_new();
void qcap2_video_encoder_dynamic_property_delete(qcap2_video_encoder_dynamic_property_t* pThis);
void qcap2_video_encoder_dynamic_get_property(qcap2_video_encoder_dynamic_property_t* pThis, ULONG * pRecordMode, ULONG * pQuality, ULONG * pBitRate, ULONG * pGOP);
void qcap2_video_encoder_dynamic_set_property(qcap2_video_encoder_dynamic_property_t* pThis, ULONG   nRecordMode, ULONG   nQuality, ULONG   nBitRate, ULONG   nGOP);

// qcap2_media_info_t
qcap2_media_info_t* qcap2_media_info_lock_from(qcap2_rcbuffer_t* pMediaInfo);
int qcap2_media_info_get_video_count(qcap2_media_info_t* pThis);
int qcap2_media_info_get_audio_count(qcap2_media_info_t* pThis);
int qcap2_media_info_get_video_encoder_count(qcap2_media_info_t* pThis);
int qcap2_media_info_get_audio_encoder_count(qcap2_media_info_t* pThis);
qcap2_video_format_t* qcap2_media_info_get_video_format(qcap2_media_info_t* pThis, int nIndex);
qcap2_audio_format_t* qcap2_media_info_get_audio_format(qcap2_media_info_t* pThis, int nIndex);
qcap2_video_encoder_property_t* qcap2_media_info_get_video_encoder_property(qcap2_media_info_t* pThis, int nIndex);
qcap2_audio_encoder_property_t* qcap2_media_info_get_audio_encoder_property(qcap2_media_info_t* pThis, int nIndex);

// qcap2_program_info_t
qcap2_program_info_t* qcap2_program_info_new();
void qcap2_program_info_delete(qcap2_program_info_t* pThis);
int qcap2_program_info_get_id(qcap2_program_info_t* pThis);
void qcap2_program_info_set_id(qcap2_program_info_t* pThis, int nId);
int qcap2_program_info_get_number(qcap2_program_info_t* pThis);
void qcap2_program_info_set_number(qcap2_program_info_t* pThis, int nNumber);
int qcap2_program_info_get_video_source_count(qcap2_program_info_t* pThis);
void qcap2_program_info_set_video_source_count(qcap2_program_info_t* pThis, int nCount);
int qcap2_program_info_get_video_source_index(qcap2_program_info_t* pThis, int nIndex);
void qcap2_program_info_set_video_source_index(qcap2_program_info_t* pThis, int nIndex, int nValue);
int qcap2_program_info_get_audio_source_count(qcap2_program_info_t* pThis);
void qcap2_program_info_set_audio_source_count(qcap2_program_info_t* pThis, int nCount);
int qcap2_program_info_get_audio_source_index(qcap2_program_info_t* pThis, int nIndex);
void qcap2_program_info_set_audio_source_index(qcap2_program_info_t* pThis, int nIndex, int nValue);
int qcap2_program_info_get_video_encoder_count(qcap2_program_info_t* pThis);
void qcap2_program_info_set_video_encoder_count(qcap2_program_info_t* pThis, int nCount);
int qcap2_program_info_get_video_encoder_index(qcap2_program_info_t* pThis, int nIndex);
void qcap2_program_info_set_video_encoder_index(qcap2_program_info_t* pThis, int nIndex, int nValue);
int qcap2_program_info_get_audio_encoder_count(qcap2_program_info_t* pThis);
void qcap2_program_info_set_audio_encoder_count(qcap2_program_info_t* pThis, int nCount);
int qcap2_program_info_get_audio_encoder_index(qcap2_program_info_t* pThis, int nIndex);
void qcap2_program_info_set_audio_encoder_index(qcap2_program_info_t* pThis, int nIndex, int nValue);
int qcap2_program_info_get_video_sink_count(qcap2_program_info_t* pThis);
void qcap2_program_info_set_video_sink_count(qcap2_program_info_t* pThis, int nCount);
int qcap2_program_info_get_video_sink_index(qcap2_program_info_t* pThis, int nIndex);
void qcap2_program_info_set_video_sink_index(qcap2_program_info_t* pThis, int nIndex, int nValue);
int qcap2_program_info_get_audio_sink_count(qcap2_program_info_t* pThis);
void qcap2_program_info_set_audio_sink_count(qcap2_program_info_t* pThis, int nCount);
int qcap2_program_info_get_audio_sink_index(qcap2_program_info_t* pThis, int nIndex);
void qcap2_program_info_set_audio_sink_index(qcap2_program_info_t* pThis, int nIndex, int nValue);
int qcap2_program_info_get_video_decoder_count(qcap2_program_info_t* pThis);
void qcap2_program_info_set_video_decoder_count(qcap2_program_info_t* pThis, int nCount);
int qcap2_program_info_get_video_decoder_index(qcap2_program_info_t* pThis, int nIndex);
void qcap2_program_info_set_video_decoder_index(qcap2_program_info_t* pThis, int nIndex, int nValue);
int qcap2_program_info_get_audio_decoder_count(qcap2_program_info_t* pThis);
void qcap2_program_info_set_audio_decoder_count(qcap2_program_info_t* pThis, int nCount);
int qcap2_program_info_get_audio_decoder_index(qcap2_program_info_t* pThis, int nIndex);
void qcap2_program_info_set_audio_decoder_index(qcap2_program_info_t* pThis, int nIndex, int nValue);
const char* qcap2_program_info_get_metadata(qcap2_program_info_t* pThis, const char* strKey);
void qcap2_program_info_set_metadata(qcap2_program_info_t* pThis, const char* strKey, const char* strValue);

// qcap2_benaphore_lock_t
qcap2_benaphore_lock_t* qcap2_benaphore_lock_new();
void qcap2_benaphore_lock_delete(qcap2_benaphore_lock_t* pThis);
void qcap2_benaphore_lock_lock(qcap2_benaphore_lock_t* pThis);
void qcap2_benaphore_lock_unlock(qcap2_benaphore_lock_t* pThis);

// qcap2_video_scaler_t
qcap2_video_scaler_t* qcap2_video_scaler_new();
void qcap2_video_scaler_delete(qcap2_video_scaler_t* pThis);
void qcap2_video_scaler_set_video_format(qcap2_video_scaler_t* pThis, qcap2_video_format_t* pVideoFormat);
void qcap2_video_scaler_get_video_format(qcap2_video_scaler_t* pThis, qcap2_video_format_t* pVideoFormat);
void qcap2_video_scaler_set_backend_type(qcap2_video_scaler_t* pThis, int nBackendType);
void qcap2_video_scaler_set_frame_count(qcap2_video_scaler_t* pThis, int nFrameCount);
void qcap2_video_scaler_set_frame_align(qcap2_video_scaler_t* pThis, int nFrameAlign);
void qcap2_video_scaler_set_frame_valign(qcap2_video_scaler_t* pThis, int nFrameVAlign);
void qcap2_video_scaler_set_multithread(qcap2_video_scaler_t* pThis, bool bMultiThread);
void qcap2_video_scaler_set_auto_run(qcap2_video_scaler_t* pThis, bool bAutoRun);
void qcap2_video_scaler_set_event(qcap2_video_scaler_t* pThis, qcap2_event_t* pEvent);
void qcap2_video_scaler_set_buffers(qcap2_video_scaler_t* pThis, qcap2_rcbuffer_t** pBuffers);
void qcap2_video_scaler_set_src_color_space(qcap2_video_scaler_t* pThis, int nSrcColorSpace); // refer to qcap2_color_space_t
void qcap2_video_scaler_set_dst_color_range(qcap2_video_scaler_t* pThis, int nDstColorRange); // refer to qcap2_color_range_t
void qcap2_video_scaler_set_src_ss_type(qcap2_video_scaler_t* pThis, int nSrcSSType); // refer to qcap2_stereoscopic_type_t
void qcap2_video_scaler_set_dst_ss_type(qcap2_video_scaler_t* pThis, int nDstSSType); // refer to qcap2_stereoscopic_type_t
void qcap2_video_scaler_set_crop(qcap2_video_scaler_t* pThis, int x, int y, int w, int h); // to crop on source video frame
void qcap2_video_scaler_set_src_buffer_hint(qcap2_video_scaler_t* pThis, int nSrcBufferHint); // refer to qcap2_buffer_hint_t
void qcap2_video_scaler_set_dst_buffer_hint(qcap2_video_scaler_t* pThis, int nDstBufferHint); // refer to qcap2_buffer_hint_t
void qcap2_video_scaler_set_filter_graph(qcap2_video_scaler_t* pThis, const char* strFilterGraph); // refer to https://ffmpeg.org/ffmpeg-filters.html
QRESULT qcap2_video_scaler_start(qcap2_video_scaler_t* pThis);
QRESULT qcap2_video_scaler_stop(qcap2_video_scaler_t* pThis);
QRESULT qcap2_video_scaler_run(qcap2_video_scaler_t* pThis);
QRESULT qcap2_video_scaler_push(qcap2_video_scaler_t* pThis, qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_video_scaler_pop(qcap2_video_scaler_t* pThis, qcap2_rcbuffer_t** ppRCBuffer);

// qcap2_audio_resampler_t
qcap2_audio_resampler_t* qcap2_audio_resampler_new();
void qcap2_audio_resampler_delete(qcap2_audio_resampler_t* pThis);
void qcap2_audio_resampler_set_audio_property(qcap2_audio_resampler_t* pThis, ULONG nChannels, ULONG nSampleFmt, ULONG nSampleFrequency, ULONG nFrameSize);
void qcap2_audio_resampler_set_frame_count(qcap2_audio_resampler_t* pThis, int nFrameCount);
void qcap2_audio_resampler_set_frame_align(qcap2_audio_resampler_t* pThis, int nFrameAlign);
void qcap2_audio_resampler_set_multithread(qcap2_audio_resampler_t* pThis, bool bMultiThread);
void qcap2_audio_resampler_set_event(qcap2_audio_resampler_t* pThis, qcap2_event_t* pEvent);
QRESULT qcap2_audio_resampler_start(qcap2_audio_resampler_t* pThis);
QRESULT qcap2_audio_resampler_stop(qcap2_audio_resampler_t* pThis);
QRESULT qcap2_audio_resampler_push(qcap2_audio_resampler_t* pThis, qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_audio_resampler_pop(qcap2_audio_resampler_t* pThis, qcap2_rcbuffer_t** ppRCBuffer);

// qcap2_video_encoder_t
qcap2_video_encoder_t* qcap2_video_encoder_new();
void qcap2_video_encoder_delete(qcap2_video_encoder_t* pThis);
void qcap2_video_encoder_set_video_property(qcap2_video_encoder_t* pThis, qcap2_video_encoder_property_t* pVideoEncoderProperty);
void qcap2_video_encoder_get_video_property(qcap2_video_encoder_t* pThis, qcap2_video_encoder_property_t* pVideoEncoderProperty);
void qcap2_video_encoder_set_dynamic_video_property(qcap2_video_encoder_t* pThis, qcap2_video_encoder_dynamic_property_t* pVideoEncoderDynamicProperty);
void qcap2_video_encoder_get_dynamic_video_property(qcap2_video_encoder_t* pThis, qcap2_video_encoder_dynamic_property_t* pVideoEncoderDynamicProperty);
void qcap2_video_encoder_get_extra_data(qcap2_video_encoder_t* pThis, uint8_t** ppExtraData, int* pExtraDataSize);
void qcap2_video_encoder_set_extra_data(qcap2_video_encoder_t* pThis, uint8_t* pExtraData, int nExtraDataSize);
void qcap2_video_encoder_set_frame_count(qcap2_video_encoder_t* pThis, int nFrameCount);
void qcap2_video_encoder_set_frame_align(qcap2_video_encoder_t* pThis, int nFrameAlign);
void qcap2_video_encoder_set_frame_valign(qcap2_video_encoder_t* pThis, int nFrameVAlign);
void qcap2_video_encoder_set_packet_count(qcap2_video_encoder_t* pThis, int nPacketCount);
void qcap2_video_encoder_set_max_packet_size(qcap2_video_encoder_t* pThis, int nMaxPacketSize);
void qcap2_video_encoder_set_multithread(qcap2_video_encoder_t* pThis, bool bMultiThread);
void qcap2_video_encoder_set_event(qcap2_video_encoder_t* pThis, qcap2_event_t* pEvent);
void qcap2_video_encoder_set_num_cores(qcap2_video_encoder_t* pThis, int nNumCores);
void qcap2_video_encoder_set_native_buffer(qcap2_video_encoder_t* pThis, bool bNativeBuffer);
void qcap2_video_encoder_request_idr(qcap2_video_encoder_t* pThis);
QRESULT qcap2_video_encoder_start(qcap2_video_encoder_t* pThis);
QRESULT qcap2_video_encoder_stop(qcap2_video_encoder_t* pThis);
QRESULT qcap2_video_encoder_push(qcap2_video_encoder_t* pThis, qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_video_encoder_pop(qcap2_video_encoder_t* pThis, qcap2_rcbuffer_t** ppRCBuffer);

// qcap2_video_decoder_t
qcap2_video_decoder_t* qcap2_video_decoder_new();
void qcap2_video_decoder_delete(qcap2_video_decoder_t* pThis);
void qcap2_video_decoder_set_video_property(qcap2_video_decoder_t* pThis, qcap2_video_encoder_property_t* pVideoEncoderProperty);
void qcap2_video_decoder_get_video_property(qcap2_video_decoder_t* pThis, qcap2_video_encoder_property_t* pVideoEncoderProperty);
void qcap2_video_decoder_get_extra_data(qcap2_video_decoder_t* pThis, uint8_t** ppExtraData, int* pExtraDataSize);
void qcap2_video_decoder_set_extra_data(qcap2_video_decoder_t* pThis, uint8_t* pExtraData, int nExtraDataSize);
void qcap2_video_decoder_set_frame_count(qcap2_video_decoder_t* pThis, int nFrameCount);
void qcap2_video_decoder_set_frame_align(qcap2_video_decoder_t* pThis, int nFrameAlign);
void qcap2_video_decoder_set_frame_valign(qcap2_video_decoder_t* pThis, int nFrameVAlign);
void qcap2_video_decoder_set_packet_count(qcap2_video_decoder_t* pThis, int nPacketCount);
void qcap2_video_decoder_set_max_packet_size(qcap2_video_decoder_t* pThis, int nMaxPacketSize);
void qcap2_video_decoder_set_multithread(qcap2_video_decoder_t* pThis, bool bMultiThread);
void qcap2_video_decoder_set_event(qcap2_video_decoder_t* pThis, qcap2_event_t* pEvent);
void qcap2_video_decoder_set_buffers(qcap2_video_decoder_t* pThis, qcap2_rcbuffer_t** pBuffers);
void qcap2_video_decoder_set_payload_type(qcap2_video_decoder_t* pThis, int nPayloadType);
QRESULT qcap2_video_decoder_start(qcap2_video_decoder_t* pThis);
QRESULT qcap2_video_decoder_stop(qcap2_video_decoder_t* pThis);
QRESULT qcap2_video_decoder_push(qcap2_video_decoder_t* pThis, qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_video_decoder_pop(qcap2_video_decoder_t* pThis, qcap2_rcbuffer_t** ppRCBuffer);

// qcap2_audio_encoder_t
qcap2_audio_encoder_t* qcap2_audio_encoder_new();
void qcap2_audio_encoder_delete(qcap2_audio_encoder_t* pThis);
void qcap2_audio_encoder_set_audio_property(qcap2_audio_encoder_t* pThis, qcap2_audio_encoder_property_t* pAudioEncoderProperty);
void qcap2_audio_encoder_get_audio_property(qcap2_audio_encoder_t* pThis, qcap2_audio_encoder_property_t* pAudioEncoderProperty);
void qcap2_audio_encoder_get_extra_data(qcap2_audio_encoder_t* pThis, uint8_t** ppExtraData, int* pExtraDataSize);
void qcap2_audio_encoder_set_extra_data(qcap2_audio_encoder_t* pThis, uint8_t* pExtraData, int nExtraDataSize);
void qcap2_audio_encoder_set_frame_count(qcap2_audio_encoder_t* pThis, int nFrameCount);
void qcap2_audio_encoder_set_packet_count(qcap2_audio_encoder_t* pThis, int nPacketCount);
void qcap2_audio_encoder_set_multithread(qcap2_audio_encoder_t* pThis, bool bMultiThread);
void qcap2_audio_encoder_set_event(qcap2_audio_encoder_t* pThis, qcap2_event_t* pEvent);
QRESULT qcap2_audio_encoder_start(qcap2_audio_encoder_t* pThis);
QRESULT qcap2_audio_encoder_stop(qcap2_audio_encoder_t* pThis);
QRESULT qcap2_audio_encoder_push(qcap2_audio_encoder_t* pThis, qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_audio_encoder_pop(qcap2_audio_encoder_t* pThis, qcap2_rcbuffer_t** ppRCBuffer);

// qcap2_audio_decoder_t
qcap2_audio_decoder_t* qcap2_audio_decoder_new();
void qcap2_audio_decoder_delete(qcap2_audio_decoder_t* pThis);
void qcap2_audio_decoder_set_audio_property(qcap2_audio_decoder_t* pThis, qcap2_audio_encoder_property_t* pAudioEncoderProperty);
void qcap2_audio_decoder_get_audio_property(qcap2_audio_decoder_t* pThis, qcap2_audio_encoder_property_t* pAudioEncoderProperty);
void qcap2_audio_decoder_get_extra_data(qcap2_audio_decoder_t* pThis, uint8_t** ppExtraData, int* pExtraDataSize);
void qcap2_audio_decoder_set_extra_data(qcap2_audio_decoder_t* pThis, uint8_t* pExtraData, int nExtraDataSize);
void qcap2_audio_decoder_set_frame_count(qcap2_audio_decoder_t* pThis, int nFrameCount);
void qcap2_audio_decoder_set_packet_count(qcap2_audio_decoder_t* pThis, int nPacketCount);
void qcap2_audio_decoder_set_multithread(qcap2_audio_decoder_t* pThis, bool bMultiThread);
void qcap2_audio_decoder_set_event(qcap2_audio_decoder_t* pThis, qcap2_event_t* pEvent);
void qcap2_audio_decoder_set_buffers(qcap2_audio_decoder_t* pThis, qcap2_rcbuffer_t** pBuffers);
void qcap2_audio_decoder_set_payload_type(qcap2_audio_decoder_t* pThis, int nPayloadType);
QRESULT qcap2_audio_decoder_start(qcap2_audio_decoder_t* pThis);
QRESULT qcap2_audio_decoder_stop(qcap2_audio_decoder_t* pThis);
QRESULT qcap2_audio_decoder_push(qcap2_audio_decoder_t* pThis, qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_audio_decoder_pop(qcap2_audio_decoder_t* pThis, qcap2_rcbuffer_t** ppRCBuffer);

// qcap2_frame_pool_t
qcap2_frame_pool_t* qcap2_frame_pool_new();
void qcap2_frame_pool_delete(qcap2_frame_pool_t* pThis);
void qcap2_frame_pool_set_backend_type(qcap2_frame_pool_t* pThis, int nBackendType);
void qcap2_frame_pool_set_frame_count(qcap2_frame_pool_t* pThis, int nFrameCount);
void qcap2_frame_pool_set_video_frame_align(qcap2_frame_pool_t* pThis, int nFrameAlign, int nFrameVAlign);
void qcap2_frame_pool_set_audio_frame_align(qcap2_frame_pool_t* pThis, int nFrameAlign);
void qcap2_frame_pool_set_video_property(qcap2_frame_pool_t* pThis, ULONG nColorSpaceType, ULONG nFrameWidth, ULONG nFrameHeight);
void qcap2_frame_pool_set_video_property1(qcap2_frame_pool_t* pThis, ULONG nWidthBorder, ULONG nHeightBorder, BOOL bMapped);
void qcap2_frame_pool_set_audio_property(qcap2_frame_pool_t* pThis, ULONG nChannels, ULONG nSampleFmt, ULONG nSampleFrequency, ULONG nAudioFrameSize);
QRESULT qcap2_frame_pool_start(qcap2_frame_pool_t* pThis);
QRESULT qcap2_frame_pool_stop(qcap2_frame_pool_t* pThis);
QRESULT qcap2_frame_pool_get_buffer(qcap2_frame_pool_t* pThis, qcap2_rcbuffer_t** ppBuffer);

// qcap2_packet_pool_t
qcap2_packet_pool_t* qcap2_packet_pool_new();
void qcap2_packet_pool_delete(qcap2_packet_pool_t* pThis);
void qcap2_packet_pool_set_packet_count(qcap2_packet_pool_t* pThis, int nPacketCount);
QRESULT qcap2_packet_pool_start(qcap2_packet_pool_t* pThis);
QRESULT qcap2_packet_pool_stop(qcap2_packet_pool_t* pThis);
QRESULT qcap2_packet_pool_get_buffer(qcap2_packet_pool_t* pThis, int nPacketSize, qcap2_rcbuffer_t** ppBuffer);

// qcap2_event_t
qcap2_event_t* qcap2_event_new();
void qcap2_event_delete(qcap2_event_t* pThis);
void qcap2_event_set_initial(qcap2_event_t* pThis, int nInitial);
QRESULT qcap2_event_start(qcap2_event_t* pThis);
QRESULT qcap2_event_stop(qcap2_event_t* pThis);
QRESULT qcap2_event_get_native_handle(qcap2_event_t* pThis, uintptr_t* pHandle);
QRESULT qcap2_event_notify(qcap2_event_t* pThis);
QRESULT qcap2_event_wait(qcap2_event_t* pThis);

// qcap2_event_handlers_t
qcap2_event_handlers_t* qcap2_event_handlers_new();
void qcap2_event_handlers_delete(qcap2_event_handlers_t* pThis);
QRESULT qcap2_event_handlers_start(qcap2_event_handlers_t* pThis);
QRESULT qcap2_event_handlers_stop(qcap2_event_handlers_t* pThis);
QRESULT qcap2_event_handlers_add_handler(qcap2_event_handlers_t* pThis, uintptr_t nHandle, qcap2_on_event_t pOnEvent, PVOID pUserData);
QRESULT qcap2_event_handlers_remove_handler(qcap2_event_handlers_t* pThis, uintptr_t nHandle);
QRESULT qcap2_event_handlers_invoke(qcap2_event_handlers_t* pThis, qcap2_on_event_t pOnEvent, PVOID pUserData);

// qcap2_rcbuffer_queue_t
qcap2_rcbuffer_queue_t* qcap2_rcbuffer_queue_new();
void qcap2_rcbuffer_queue_delete(qcap2_rcbuffer_queue_t* pThis);
void qcap2_rcbuffer_queue_set_max_buffers(qcap2_rcbuffer_queue_t* pThis, int nMaxBuffers);
void qcap2_rcbuffer_queue_set_event(qcap2_rcbuffer_queue_t* pThis, qcap2_event_t* pEvent);
void qcap2_rcbuffer_queue_set_buffers(qcap2_rcbuffer_queue_t* pThis, qcap2_rcbuffer_t** pBuffers);
int qcap2_rcbuffer_queue_get_buffer_count(qcap2_rcbuffer_queue_t* pThis);
bool qcap2_rcbuffer_queue_is_full(qcap2_rcbuffer_queue_t* pThis);
bool qcap2_rcbuffer_queue_is_empty(qcap2_rcbuffer_queue_t* pThis);
QRESULT qcap2_rcbuffer_queue_start(qcap2_rcbuffer_queue_t* pThis);
QRESULT qcap2_rcbuffer_queue_stop(qcap2_rcbuffer_queue_t* pThis);
QRESULT qcap2_rcbuffer_queue_push(qcap2_rcbuffer_queue_t* pThis, qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_rcbuffer_queue_pop(qcap2_rcbuffer_queue_t* pThis, qcap2_rcbuffer_t** ppRCBuffer);

// qcap2_timer_t
qcap2_timer_t* qcap2_timer_new();
void qcap2_timer_delete(qcap2_timer_t* pThis);
void qcap2_timer_set_interval(qcap2_timer_t* pThis, uint64_t nInterval);
QRESULT qcap2_timer_start(qcap2_timer_t* pThis);
QRESULT qcap2_timer_stop(qcap2_timer_t* pThis);
QRESULT qcap2_timer_get_native_handle(qcap2_timer_t* pThis, uintptr_t* pHandle);
QRESULT qcap2_timer_wait(qcap2_timer_t* pThis, uint64_t* pExpirations);
QRESULT qcap2_timer_next(qcap2_timer_t* pThis, uint64_t nDuration);

// qcap2_window_t
qcap2_window_t* qcap2_window_new();
void qcap2_window_delete(qcap2_window_t* pThis);
void qcap2_window_set_backend_type(qcap2_window_t* pThis, int nType);
void qcap2_window_set_rect(qcap2_window_t* pThis, int x, int y, int w, int h);
void qcap2_window_set_full_screen(qcap2_window_t* pThis, bool bFullScreen);
QRESULT qcap2_window_start(qcap2_window_t* pThis);
QRESULT qcap2_window_stop(qcap2_window_t* pThis);
QRESULT qcap2_window_get_hwnd(qcap2_window_t* pThis, HWND* pHwnd);
QRESULT qcap2_window_get_native_handle(qcap2_window_t* pThis, uintptr_t* pHandle);
QRESULT qcap2_window_handle_events(qcap2_window_t* pThis);

// qcap2_block_lock_t
qcap2_block_lock_t* qcap2_block_lock_new();
void qcap2_block_lock_delete(qcap2_block_lock_t* pThis);
void qcap2_block_lock_grant(qcap2_block_lock_t* pThis, bool bGranted);
bool qcap2_block_lock_enter(qcap2_block_lock_t* pThis);
void qcap2_block_lock_leave(qcap2_block_lock_t* pThis);

// qcap2_binder_t
qcap2_binder_t* qcap2_binder_new();
void qcap2_binder_delete(qcap2_binder_t* pThis);
QRESULT qcap2_binder_start(qcap2_binder_t* pThis);
QRESULT qcap2_binder_stop(qcap2_binder_t* pThis);
intptr_t qcap2_binder_vsrc_vsca(qcap2_binder_t* pThis, qcap2_video_source_t* pSrc, qcap2_video_scaler_t* pSink);
QRESULT qcap2_binder_unlink(qcap2_binder_t* pThis, intptr_t cookie);

// qcap2_qdev_enum_t
qcap2_qdev_enum_t* qcap2_qdev_enum_new();
void qcap2_qdev_enum_delete(qcap2_qdev_enum_t* pThis);
void qcap2_qdev_enum_set_type(qcap2_qdev_enum_t* pThis, int nType);
void qcap2_qdev_enum_set_event(qcap2_qdev_enum_t* pThis, qcap2_event_t* pEvent);
QRESULT qcap2_qdev_enum_start(qcap2_qdev_enum_t* pThis);
QRESULT qcap2_qdev_enum_stop(qcap2_qdev_enum_t* pThis);
QRESULT qcap2_qdev_enum_pop(qcap2_qdev_enum_t* pThis, qcap2_rcbuffer_t** ppQdevInfo); // qcap2_qdev_info_t

// qcap2_qdev_info_t
qcap2_qdev_info_t* qcap2_qdev_info_lock_from(qcap2_rcbuffer_t* pQdevInfo);
int qcap2_qdev_info_get_type(qcap2_qdev_info_t* pThis);
uint32_t qcap2_qdev_info_get_uid(qcap2_qdev_info_t* pThis);
BOOL qcap2_qdev_info_get_plugged(qcap2_qdev_info_t* pThis);
const char* qcap2_qdev_info_get_path(qcap2_qdev_info_t* pThis);
int qcap2_qdev_info_get_vendor_id(qcap2_qdev_info_t* pThis);
int qcap2_qdev_info_get_device_id(qcap2_qdev_info_t* pThis);

// qcap2_qdev_t
qcap2_qdev_t* qcap2_qdev_new();
void qcap2_qdev_delete(qcap2_qdev_t* pThis);
void qcap2_qdev_set_event(qcap2_qdev_t* pThis, qcap2_event_t* pEvent);
void qcap2_qdev_set_poll_duration(qcap2_qdev_t* pThis, int nPollDuration);
void qcap2_qdev_set_info(qcap2_qdev_t* pThis, qcap2_rcbuffer_t* pQdevInfo); // qcap2_qdev_info_t
QRESULT qcap2_qdev_start(qcap2_qdev_t* pThis);
QRESULT qcap2_qdev_stop(qcap2_qdev_t* pThis);
QRESULT qcap2_qdev_get_video_count(qcap2_qdev_t* pThis, int* pCount);
QRESULT qcap2_qdev_get_audio_count(qcap2_qdev_t* pThis, int* pCount);
QRESULT qcap2_qdev_get_video_encoder_count(qcap2_qdev_t* pThis, int* pCount);
QRESULT qcap2_qdev_get_audio_encoder_count(qcap2_qdev_t* pThis, int* pCount);
QRESULT qcap2_qdev_get_video_input(qcap2_qdev_t* pThis, int nIndex, ULONG* pVideoInput);
QRESULT qcap2_qdev_set_video_input(qcap2_qdev_t* pThis, int nIndex, ULONG nVideoInput);
QRESULT qcap2_qdev_get_audio_input(qcap2_qdev_t* pThis, int nIndex, ULONG* pAudioInput);
QRESULT qcap2_qdev_set_audio_input(qcap2_qdev_t* pThis, int nIndex, ULONG nAudioInput);
QRESULT qcap2_qdev_config_video_source(qcap2_qdev_t* pThis, int nIndex, qcap2_video_source_t* pVideoSource);
QRESULT qcap2_qdev_pop(qcap2_qdev_t* pThis, qcap2_rcbuffer_t** ppMediaInfo); // qcap2_media_info_t

// qcap2_video_source_t
qcap2_video_source_t* qcap2_video_source_new();
void qcap2_video_source_delete(qcap2_video_source_t* pThis);
void qcap2_video_source_set_backend_type(qcap2_video_source_t* pThis, int nBackendType);
void qcap2_video_source_set_frame_count(qcap2_video_source_t* pThis, int nFrameCount);
void qcap2_video_source_set_event(qcap2_video_source_t* pThis, qcap2_event_t* pEvent);
void qcap2_video_source_set_auto_run(qcap2_video_source_t* pThis, bool bAutoRun);
void qcap2_video_source_set_video_format(qcap2_video_source_t* pThis, qcap2_video_format_t* pVideoFormat);
void qcap2_video_source_get_video_format(qcap2_video_source_t* pThis, qcap2_video_format_t* pVideoFormat);
void qcap2_video_source_set_buffers(qcap2_video_source_t* pThis, qcap2_rcbuffer_t** pBuffers);
void qcap2_video_source_set_device_index(qcap2_video_source_t* pThis, int nDeviceIndex);
void qcap2_video_source_set_stream_index(qcap2_video_source_t* pThis, int nStreamIndex);
void qcap2_video_source_set_src_ss_type(qcap2_video_source_t* pThis, int nSrcSSType); // refer to qcap2_stereoscopic_type_t
void qcap2_video_source_set_dst_ss_type(qcap2_video_source_t* pThis, int nDstSSType); // refer to qcap2_stereoscopic_type_t
QRESULT qcap2_video_source_start(qcap2_video_source_t* pThis);
QRESULT qcap2_video_source_stop(qcap2_video_source_t* pThis);
QRESULT qcap2_video_source_run(qcap2_video_source_t* pThis);
QRESULT qcap2_video_source_pop(qcap2_video_source_t* pThis, qcap2_rcbuffer_t** ppRCBuffer);

// qcap2_video_sink_t
qcap2_video_sink_t* qcap2_video_sink_new();
void qcap2_video_sink_delete(qcap2_video_sink_t* pThis);
void qcap2_video_sink_set_backend_type(qcap2_video_sink_t* pThis, int nBackendType);
void qcap2_video_sink_set_video_format(qcap2_video_sink_t* pThis, qcap2_video_format_t* pVideoFormat);
void qcap2_video_sink_set_auto_run(qcap2_video_sink_t* pThis, bool bAutoRun);
void qcap2_video_sink_set_multithread(qcap2_video_sink_t* pThis, bool bMultiThread);
void qcap2_video_sink_set_frame_count(qcap2_video_sink_t* pThis, int nFrameCount);
void qcap2_video_sink_set_native_handle(qcap2_video_sink_t* pThis, uintptr_t nHandle); // window handle
void qcap2_video_sink_set_low_bandwidth(qcap2_video_sink_t* pThis, bool bLowBandwidth);
void qcap2_video_sink_set_display_system(qcap2_video_sink_t* pThis, int nDisplaySystem);
void qcap2_video_sink_set_graphic_window_system(qcap2_video_sink_t* pThis, int nGraphicWindowSystem);
void qcap2_video_sink_set_gpu_direct(qcap2_video_sink_t* pThis, bool bGPUDirect);
void qcap2_video_sink_set_scale_style(qcap2_video_sink_t* pThis, ULONG nScaleStyle); // refer to QCAP_SCALE_STYLE_*
void qcap2_video_sink_set_device_index(qcap2_video_sink_t* pThis, int nDeviceIndex);
void qcap2_video_sink_set_src_ss_type(qcap2_video_sink_t* pThis, int nSrcSSType); // refer to qcap2_stereoscopic_type_t
void qcap2_video_sink_set_dst_ss_type(qcap2_video_sink_t* pThis, int nDstSSType); // refer to qcap2_stereoscopic_type_t
QRESULT qcap2_video_sink_start(qcap2_video_sink_t* pThis);
QRESULT qcap2_video_sink_stop(qcap2_video_sink_t* pThis);
QRESULT qcap2_video_sink_run(qcap2_video_sink_t* pThis);
QRESULT qcap2_video_sink_push(qcap2_video_sink_t* pThis, qcap2_rcbuffer_t* pRCBuffer);

// qcap2_audio_source_t
qcap2_audio_source_t* qcap2_audio_source_new();
void qcap2_audio_source_delete(qcap2_audio_source_t* pThis);
void qcap2_audio_source_set_backend_type(qcap2_audio_source_t* pThis, int nBackendType);
void qcap2_audio_source_set_frame_count(qcap2_audio_source_t* pThis, int nFrameCount);
void qcap2_audio_source_set_event(qcap2_audio_source_t* pThis, qcap2_event_t* pEvent);
void qcap2_audio_source_set_audio_format(qcap2_audio_source_t* pThis, qcap2_audio_format_t* pAudioFormat);
void qcap2_audio_source_get_audio_format(qcap2_audio_source_t* pThis, qcap2_audio_format_t* pAudioFormat);
void qcap2_audio_source_set_buffers(qcap2_audio_source_t* pThis, qcap2_rcbuffer_t** pBuffers);
void qcap2_audio_source_set_period_time(qcap2_audio_source_t* pThis, int nPeriodTime);
void qcap2_audio_source_set_buffer_time(qcap2_audio_source_t* pThis, int nBufferTime);
void qcap2_audio_source_set_ideal_timer(qcap2_audio_source_t* pThis, bool bIdealTimer);
void qcap2_audio_source_set_test_pattern(qcap2_audio_source_t* pThis, int nType); // qcap2_audio_test_pattern_type_t; TPG backend only
void qcap2_audio_source_set_card(qcap2_audio_source_t* pThis, int nCard);
void qcap2_audio_source_set_device(qcap2_audio_source_t* pThis, int nDevice);
QRESULT qcap2_audio_source_start(qcap2_audio_source_t* pThis);
QRESULT qcap2_audio_source_stop(qcap2_audio_source_t* pThis);
QRESULT qcap2_audio_source_pop(qcap2_audio_source_t* pThis, qcap2_rcbuffer_t** ppRCBuffer);

// qcap2_audio_sink_t
qcap2_audio_sink_t* qcap2_audio_sink_new();
void qcap2_audio_sink_delete(qcap2_audio_sink_t* pThis);
void qcap2_audio_sink_set_backend_type(qcap2_audio_sink_t* pThis, int nBackendType);
void qcap2_audio_sink_set_audio_format(qcap2_audio_sink_t* pThis, qcap2_audio_format_t* pAudioFormat);
void qcap2_audio_sink_set_period_time(qcap2_audio_sink_t* pThis, int nPeriodTime);
void qcap2_audio_sink_set_buffer_time(qcap2_audio_sink_t* pThis, int nBufferTime);
void qcap2_audio_sink_set_card(qcap2_audio_sink_t* pThis, int nCard);
void qcap2_audio_sink_set_device(qcap2_audio_sink_t* pThis, int nDevice);
QRESULT qcap2_audio_sink_start(qcap2_audio_sink_t* pThis);
QRESULT qcap2_audio_sink_stop(qcap2_audio_sink_t* pThis);
QRESULT qcap2_audio_sink_push(qcap2_audio_sink_t* pThis, qcap2_rcbuffer_t* pRCBuffer);

// qcap2_demuxer_t
qcap2_demuxer_t* qcap2_demuxer_new();
void qcap2_demuxer_delete(qcap2_demuxer_t* pThis);
void qcap2_demuxer_set_type(qcap2_demuxer_t* pThis, int nType);
void qcap2_demuxer_set_event(qcap2_demuxer_t* pThis, qcap2_event_t* pEvent);
void qcap2_demuxer_set_url(qcap2_demuxer_t* pThis, const char* strURL);
void qcap2_demuxer_set_max_buffer_length(qcap2_demuxer_t* pThis, int nMaxBufferLength); // in milliseconds
void qcap2_demuxer_set_find_stream_info(qcap2_demuxer_t* pThis, bool bFindStreamInfo);
void qcap2_demuxer_set_push_source(qcap2_demuxer_t* pThis, bool bPushSource);
void qcap2_demuxer_set_live_source(qcap2_demuxer_t* pThis, bool bLiveSource);
void qcap2_demuxer_set_format_name(qcap2_demuxer_t* pThis, const char* strFormatName); // refer to `ffprobe -demuxers`
void qcap2_demuxer_set_buffer_size(qcap2_demuxer_t* pThis, int nBufferSize); // in bytes for push-source
void qcap2_demuxer_set_sdp_lines(qcap2_demuxer_t* pThis, const char* strSdpLines);
void qcap2_demuxer_set_tcp(qcap2_demuxer_t* pThis, bool bUseTCP);
void qcap2_demuxer_set_multicast(qcap2_demuxer_t* pThis, bool bMulticast);
QRESULT qcap2_demuxer_start(qcap2_demuxer_t* pThis);
QRESULT qcap2_demuxer_stop(qcap2_demuxer_t* pThis);
QRESULT qcap2_demuxer_push(qcap2_demuxer_t* pThis, qcap2_rcbuffer_t* pRCBuffer); // for push-source
int qcap2_demuxer_get_video_source_count(qcap2_demuxer_t* pThis);
int qcap2_demuxer_get_audio_source_count(qcap2_demuxer_t* pThis);
int qcap2_demuxer_get_video_encoder_count(qcap2_demuxer_t* pThis);
int qcap2_demuxer_get_audio_encoder_count(qcap2_demuxer_t* pThis);
int qcap2_demuxer_get_program_count(qcap2_demuxer_t* pThis);
qcap2_video_source_t* qcap2_demuxer_get_video_source(qcap2_demuxer_t* pThis, int nIndex);
qcap2_audio_source_t* qcap2_demuxer_get_audio_source(qcap2_demuxer_t* pThis, int nIndex);
qcap2_video_encoder_t* qcap2_demuxer_get_video_encoder(qcap2_demuxer_t* pThis, int nIndex);
qcap2_audio_encoder_t* qcap2_demuxer_get_audio_encoder(qcap2_demuxer_t* pThis, int nIndex);
qcap2_program_info_t* qcap2_demuxer_get_program_info(qcap2_demuxer_t* pThis, int nIndex);
QRESULT qcap2_demuxer_play(qcap2_demuxer_t* pThis);
QRESULT qcap2_demuxer_update(qcap2_demuxer_t* pThis); // to sync status (progs, vsrc, asrc, venc, aenc)

// qcap2_muxer_t
qcap2_muxer_t* qcap2_muxer_new();
void qcap2_muxer_delete(qcap2_muxer_t* pThis);
void qcap2_muxer_set_type(qcap2_muxer_t* pThis, int nType);
void qcap2_muxer_set_max_threads(qcap2_muxer_t* pThis, int nMaxThreads);
void qcap2_muxer_set_endpoint(qcap2_muxer_t* pThis, const char* strIP, int nPort);
void qcap2_muxer_set_url(qcap2_muxer_t* pThis, const char* strURL); // HLS: output .m3u8 path
void qcap2_muxer_set_segment_duration(qcap2_muxer_t* pThis, int nMilliseconds); // HLS only
void qcap2_muxer_set_segment_playlist_count(qcap2_muxer_t* pThis, int nCount); // HLS only
void qcap2_muxer_set_segment_preserve_all(qcap2_muxer_t* pThis, bool bPreserveAll); // HLS only
void qcap2_muxer_set_realm(qcap2_muxer_t* pThis, const char* strRealm);
void qcap2_muxer_set_ssl(qcap2_muxer_t* pThis, bool bSSL);
void qcap2_muxer_set_certificate_chain_file(qcap2_muxer_t* pThis, const char* strCertificateChainFile);
void qcap2_muxer_set_private_key_file(qcap2_muxer_t* pThis, const char* strPrivateKeyFile);
void qcap2_muxer_set_realm(qcap2_muxer_t* pThis, const char* strRealm);
void qcap2_muxer_add_user(qcap2_muxer_t* pThis, const char* strAccount, const char* strPassword);
void qcap2_muxer_add_program_info(qcap2_muxer_t* pThis, qcap2_program_info_t* pProgramInfo);
QRESULT qcap2_muxer_start(qcap2_muxer_t* pThis);
QRESULT qcap2_muxer_stop(qcap2_muxer_t* pThis);
int qcap2_muxer_get_video_sink_count(qcap2_muxer_t* pThis);
int qcap2_muxer_get_audio_sink_count(qcap2_muxer_t* pThis);
int qcap2_muxer_get_video_decoder_count(qcap2_muxer_t* pThis);
int qcap2_muxer_get_audio_decoder_count(qcap2_muxer_t* pThis);
int qcap2_muxer_get_program_count(qcap2_muxer_t* pThis);
qcap2_program_info_t* qcap2_muxer_get_program_info(qcap2_muxer_t* pThis, int nIndex);
qcap2_video_sink_t* qcap2_muxer_get_video_sink(qcap2_muxer_t* pThis, int nIndex);
qcap2_audio_sink_t* qcap2_muxer_get_audio_sink(qcap2_muxer_t* pThis, int nIndex);
qcap2_video_decoder_t* qcap2_muxer_get_video_decoder(qcap2_muxer_t* pThis, int nIndex);
qcap2_audio_decoder_t* qcap2_muxer_get_audio_decoder(qcap2_muxer_t* pThis, int nIndex);
QRESULT qcap2_muxer_play(qcap2_muxer_t* pThis);

// qcap2_video_matte_t
qcap2_video_matte_t* qcap2_video_matte_new();
void qcap2_video_matte_delete(qcap2_video_matte_t* pThis);
void qcap2_video_matte_set_video_format(qcap2_video_matte_t* pThis, qcap2_video_format_t* pVideoFormat);
void qcap2_video_matte_get_video_format(qcap2_video_matte_t* pThis, qcap2_video_format_t* pVideoFormat);
void qcap2_video_matte_set_backend_type(qcap2_video_matte_t* pThis, int nBackendType);
void qcap2_video_matte_set_frame_count(qcap2_video_matte_t* pThis, int nFrameCount);
void qcap2_video_matte_set_buffers(qcap2_video_matte_t* pThis, qcap2_rcbuffer_t** pBuffers);
void qcap2_video_matte_set_alpha_buffers(qcap2_video_matte_t* pThis, qcap2_rcbuffer_t** pAlphaBuffers);
void qcap2_video_matte_set_params(qcap2_video_matte_t* pThis, float pParams[16]);
QRESULT qcap2_video_matte_start(qcap2_video_matte_t* pThis);
QRESULT qcap2_video_matte_stop(qcap2_video_matte_t* pThis);
QRESULT qcap2_video_matte_push(qcap2_video_matte_t* pThis, qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_video_matte_pop(qcap2_video_matte_t* pThis, qcap2_rcbuffer_t** ppRCBuffer, qcap2_rcbuffer_t** ppRCBuffer_Alpha);

// qcap2_video_blender_t
qcap2_video_blender_t* qcap2_video_blender_new();
void qcap2_video_blender_delete(qcap2_video_blender_t* pThis);
void qcap2_video_blender_set_video_format(qcap2_video_blender_t* pThis, qcap2_video_format_t* pVideoFormat);
void qcap2_video_blender_get_video_format(qcap2_video_blender_t* pThis, qcap2_video_format_t* pVideoFormat);
void qcap2_video_blender_set_backend_type(qcap2_video_blender_t* pThis, int nBackendType);
void qcap2_video_blender_set_frame_count(qcap2_video_blender_t* pThis, int nFrameCount);
void qcap2_video_blender_set_buffers(qcap2_video_blender_t* pThis, qcap2_rcbuffer_t** pBuffers);
QRESULT qcap2_video_blender_start(qcap2_video_blender_t* pThis);
QRESULT qcap2_video_blender_stop(qcap2_video_blender_t* pThis);
QRESULT qcap2_video_blender_push(qcap2_video_blender_t* pThis, qcap2_rcbuffer_t* pRCBuffer, qcap2_rcbuffer_t* pRCBuffer_cover, qcap2_rcbuffer_t* pRCBuffer_alpha);
QRESULT qcap2_video_blender_pop(qcap2_video_blender_t* pThis, qcap2_rcbuffer_t** ppRCBuffer);

// qcap2_bitstream_filter_t
qcap2_bitstream_filter_t* qcap2_bitstream_filter_new();
void qcap2_bitstream_filter_delete(qcap2_bitstream_filter_t* pThis);
void qcap2_bitstream_filter_set_backend_type(qcap2_bitstream_filter_t* pThis, int nBackendType);
void qcap2_bitstream_filter_set_video_encoder_format(qcap2_bitstream_filter_t* pThis, ULONG nEncoderFormat);
void qcap2_bitstream_filter_get_video_encoder_format(qcap2_bitstream_filter_t* pThis, ULONG* pEncoderFormat);
void qcap2_bitstream_filter_set_audio_encoder_format(qcap2_bitstream_filter_t* pThis, ULONG nEncoderFormat);
void qcap2_bitstream_filter_get_audio_encoder_format(qcap2_bitstream_filter_t* pThis, ULONG* pEncoderFormat);
void qcap2_bitstream_filter_set_packet_count(qcap2_bitstream_filter_t* pThis, int nPacketCount);
void qcap2_bitstream_filter_set_event(qcap2_bitstream_filter_t* pThis, qcap2_event_t* pEvent);
QRESULT qcap2_bitstream_filter_start(qcap2_bitstream_filter_t* pThis);
QRESULT qcap2_bitstream_filter_stop(qcap2_bitstream_filter_t* pThis);
QRESULT qcap2_bitstream_filter_push(qcap2_bitstream_filter_t* pThis, qcap2_rcbuffer_t* pRCBuffer, int n, ...);
QRESULT qcap2_bitstream_filter_pop(qcap2_bitstream_filter_t* pThis, qcap2_rcbuffer_t** ppRCBuffer, int n, ...);

// qcap2_dns_source_t
qcap2_dns_source_t* qcap2_dns_source_new();
void qcap2_dns_source_delete(qcap2_dns_source_t* pThis);
void qcap2_dns_source_set_event(qcap2_dns_source_t* pThis, qcap2_event_t* pEvent);
void qcap2_dns_source_set_listen_address(qcap2_dns_source_t* pThis, const char* strListenAddress);
void qcap2_dns_source_set_multicast_address(qcap2_dns_source_t* pThis, const char* strMulticastAddress);
void qcap2_dns_source_set_port(qcap2_dns_source_t* pThis, int nPort);
void qcap2_dns_source_set_polling_time(qcap2_dns_source_t* pThis, int nPollingTime); // in seconds
void qcap2_dns_source_set_host_name(qcap2_dns_source_t* pThis, const char* strHostName);
QRESULT qcap2_dns_source_start(qcap2_dns_source_t* pThis);
QRESULT qcap2_dns_source_stop(qcap2_dns_source_t* pThis);
QRESULT qcap2_dns_source_pop(qcap2_dns_source_t* pThis, qcap2_rcbuffer_t** ppRCBuffer); // refer to qcap2_dns_event_t

// qcap2_clock_source_t
qcap2_clock_source_t* qcap2_clock_source_new();
void qcap2_clock_source_delete(qcap2_clock_source_t* pThis);
void qcap2_clock_source_set_event(qcap2_clock_source_t* pThis, qcap2_event_t* pEvent);
void qcap2_clock_source_set_listen_address(qcap2_clock_source_t* pThis, const char* strListenAddress);
void qcap2_clock_source_set_server_address(qcap2_clock_source_t* pThis, const char* strServerAddress);
void qcap2_clock_source_set_port(qcap2_clock_source_t* pThis, int nPort);
void qcap2_clock_source_set_polling_time(qcap2_clock_source_t* pThis, int nPollingTime); // in seconds
void qcap2_clock_source_set_resync_range(qcap2_clock_source_t* pThis, int nResyncMin, int nResyncMax); // in milliseconds
QRESULT qcap2_clock_source_start(qcap2_clock_source_t* pThis);
QRESULT qcap2_clock_source_stop(qcap2_clock_source_t* pThis);
QRESULT qcap2_clock_source_pop(qcap2_clock_source_t* pThis, qcap2_rcbuffer_t** ppRCBuffer); // refer to qcap2_clock_event_t

// qcap2_font_atlas_t
qcap2_font_atlas_t* qcap2_font_atlas_new();
void qcap2_font_atlas_delete(qcap2_font_atlas_t* pThis);
void qcap2_font_atlas_set_font_file(qcap2_font_atlas_t* pThis, const char* strFontFile);
void qcap2_font_atlas_set_family_name(qcap2_font_atlas_t* pThis, const char* strFamilyName);
void qcap2_font_atlas_set_style(qcap2_font_atlas_t* pThis, int nStyle); // refer to QCAP_FONT_STYLE_XXX
void qcap2_font_atlas_set_char_size(qcap2_font_atlas_t* pThis, int nCharSize); // point size
void qcap2_font_atlas_set_dpi(qcap2_font_atlas_t* pThis, int nDPI);
void qcap2_font_atlas_set_atlas_size(qcap2_font_atlas_t* pThis, int nWidth, int nHeight);
QRESULT qcap2_font_atlas_start(qcap2_font_atlas_t* pThis);
QRESULT qcap2_font_atlas_stop(qcap2_font_atlas_t* pThis);
float qcap2_font_atlas_get_ascender(qcap2_font_atlas_t* pThis);
float qcap2_font_atlas_get_descender(qcap2_font_atlas_t* pThis);
float qcap2_font_atlas_get_height(qcap2_font_atlas_t* pThis);

// qcap2_graphics_t
qcap2_graphics_t* qcap2_graphics_new();
void qcap2_graphics_delete(qcap2_graphics_t* pThis);
void qcap2_graphics_set_backend_type(qcap2_graphics_t* pThis, int nBackendType);
QRESULT qcap2_graphics_start(qcap2_graphics_t* pThis);
QRESULT qcap2_graphics_stop(qcap2_graphics_t* pThis);
QRESULT qcap2_graphics_begin(qcap2_graphics_t* pThis, qcap2_rcbuffer_t* pRCBuffer);
QRESULT qcap2_graphics_end(qcap2_graphics_t* pThis);
void qcap2_graphics_set_font_atlas(qcap2_graphics_t* pThis, qcap2_font_atlas_t* pFontAtlas);
void qcap2_graphics_set_color(qcap2_graphics_t* pThis, int32_t nColor); // 0xAARRGGBB
QRESULT qcap2_graphics_fill_rect(qcap2_graphics_t* pThis, int x, int y, int w, int h);
QRESULT qcap2_graphics_draw_text(qcap2_graphics_t* pThis, const char* strText, int x, int y, int w, int h);
QRESULT qcap2_graphics_get_text_size(qcap2_graphics_t* pThis, const char* strText, int* x, int* y, int* w, int* h);
// QRESULT qcap2_graphics_draw_sprite(qcap2_graphics_t* pThis, qcap2_rcbuffer_t* pRCBuffer, int x, int y, int w, int h);

/*
 * Buffer V2 & Subsystem Infrastructure Extensions
 */
#include "qcap2_buffer_v2.h"
#include "qcap2_buffer_v2_bridge.h"
#include "qcap2_frame_pool_v2.h"
#include "qcap2_video_source_v2.h"
#include "qcap2_video_scaler_v2.h"

#ifndef QCAP_DEPRECATED
  #if defined(__GNUC__) || defined(__clang__)
    #define QCAP_DEPRECATED(msg) __attribute__((deprecated(msg)))
  #elif defined(_MSC_VER)
    #define QCAP_DEPRECATED(msg) __declspec(deprecated(msg))
  #else
    #define QCAP_DEPRECATED(msg)
  #endif
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __QCAP2_H__
