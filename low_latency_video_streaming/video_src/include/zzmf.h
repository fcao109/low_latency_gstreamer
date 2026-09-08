#ifndef __ZZMF_2817110C_3A76_41E7_B2F5_1EBC5BA75DA7__
#define __ZZMF_2817110C_3A76_41E7_B2F5_1EBC5BA75DA7__

#include <stdint.h>
#include <stddef.h>

#define ZZMF_EXPORT
#define ZZMF_API
#define ZZMF_CALLBACK

#define ZZMF_FOURCC(ch0, ch1, ch2, ch3) \
    ((unsigned long)(unsigned char) (ch0) | ((unsigned long)(unsigned char) (ch1) << 8) | \
    ((unsigned long)(unsigned char) (ch2) << 16) | ((unsigned long)(unsigned char) (ch3) << 24 ))

struct zzmf_tsmux_t;
struct zzmf_devinfo_t;
struct zzmf_devservice_t;
struct zzmf_sessmgr_t;
struct zzmf_devprovider_t;
struct zzmf_devenum_t;
struct zzmf_devcon_t;
struct zzmf_vpss_t;

typedef void (ZZMF_CALLBACK *zzmf_tsmux_on_packet)(uint8_t* data, size_t size, intptr_t user_data, intptr_t user_data1);
typedef void (ZZMF_CALLBACK *zzmf_devservice_on_request_t)(zzmf_devinfo_t* req, zzmf_devinfo_t* res, intptr_t user_data);
typedef void (ZZMF_CALLBACK *zzmf_devenum_on_dev_found_t)(zzmf_devcon_t* dev, intptr_t user_data);
typedef void (ZZMF_CALLBACK *zzmf_devcon_on_response_t)(zzmf_devinfo_t* info, intptr_t user_data);

#ifdef __cplusplus
extern "C" {
#endif

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// transport stream muxer
	ZZMF_EXPORT zzmf_tsmux_t* ZZMF_API zzmf_tsmux_new();
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_addref(zzmf_tsmux_t* pThis);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_release(zzmf_tsmux_t* pThis);

	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_callbacks(zzmf_tsmux_t* pThis, zzmf_tsmux_on_packet on_packet, intptr_t user_data, intptr_t user_data1);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_filename(zzmf_tsmux_t* pThis, const char* filename);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_video_property(zzmf_tsmux_t* pThis, const char* type, int width, int height, const char* pix_fmt, int fps_num, int fps_den);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_audio_property(zzmf_tsmux_t* pThis, const char* type, int channels, const char* sample_fmt, int sample_rate);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_service_name(zzmf_tsmux_t* pThis, const char* name);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_service_provider(zzmf_tsmux_t* pThis, const char* provider);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_mux_rate(zzmf_tsmux_t* pThis, int rate); // in kilobit (Kb)
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_max_delay(zzmf_tsmux_t* pThis, int max_delay); // in milliseconds
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_start_pid(zzmf_tsmux_t* pThis, int pid);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_pmt_start_pid(zzmf_tsmux_t* pThis, int pid);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_service_id(zzmf_tsmux_t* pThis, int id);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_transport_stream_id(zzmf_tsmux_t* pThis, int id);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_original_network_id(zzmf_tsmux_t* pThis, int id);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_video_pid(zzmf_tsmux_t* pThis, int pid);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_audio_pid(zzmf_tsmux_t* pThis, int pid);

	// multiple streams support
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_video_property1(zzmf_tsmux_t* pThis, int streamid, const char* type, int width, int height, const char* pix_fmt, int fps_num, int fps_den);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_audio_property1(zzmf_tsmux_t* pThis, int streamid, const char* type, int channels, const char* sample_fmt, int sample_rate);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_set_audio_property2(zzmf_tsmux_t* pThis, int streamid, const char* type, int channels, const char* sample_fmt, int sample_rate, int stream_type);

	// multiple programs support
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_program_add_stream(zzmf_tsmux_t* pThis, int progid, int streamid);

	ZZMF_EXPORT int ZZMF_API zzmf_tsmux_init(zzmf_tsmux_t* pThis);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_uninit(zzmf_tsmux_t* pThis);

	// mux
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_write_video_frame(zzmf_tsmux_t* pThis, int64_t pts, uint8_t* data, size_t size, int flags);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_write_audio_frame(zzmf_tsmux_t* pThis, int64_t pts, uint8_t* data, size_t size);

	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_write_video_frame2(zzmf_tsmux_t* pThis, int64_t pts, int64_t dts, uint8_t* data, size_t size, int flags);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_write_audio_frame2(zzmf_tsmux_t* pThis, int64_t pts, int64_t dts, uint8_t* data, size_t size);

	// multiple streams support
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_write_video_frame3(zzmf_tsmux_t* pThis, int streamid, int64_t pts, int64_t dts, uint8_t* data, size_t size, int flags);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_write_audio_frame3(zzmf_tsmux_t* pThis, int streamid, int64_t pts, int64_t dts, uint8_t* data, size_t size);

	// support pts/dts offset
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_write_video_frame4(zzmf_tsmux_t* pThis, int streamid, int64_t pts, int64_t dts, int64_t ts_offset, uint8_t* data, size_t size, int flags);
	ZZMF_EXPORT void ZZMF_API zzmf_tsmux_write_audio_frame4(zzmf_tsmux_t* pThis, int streamid, int64_t pts, int64_t dts, int64_t ts_offset, uint8_t* data, size_t size);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// device info
	ZZMF_EXPORT zzmf_devinfo_t* ZZMF_API zzmf_devinfo_new();
	ZZMF_EXPORT void ZZMF_API zzmf_devinfo_add_ref(zzmf_devinfo_t* pThis);
	ZZMF_EXPORT void ZZMF_API zzmf_devinfo_release(zzmf_devinfo_t* pThis);

	ZZMF_EXPORT void ZZMF_API zzmf_devinfo_set_i(zzmf_devinfo_t* pThis, const char* name, int value);
	ZZMF_EXPORT void ZZMF_API zzmf_devinfo_set_d(zzmf_devinfo_t* pThis, const char* name, double value);
	ZZMF_EXPORT void ZZMF_API zzmf_devinfo_set_str(zzmf_devinfo_t* pThis, const char* name, const char* value);

	ZZMF_EXPORT int ZZMF_API zzmf_devinfo_get_i(zzmf_devinfo_t* pThis, const char* name, int* ret);
	ZZMF_EXPORT int ZZMF_API zzmf_devinfo_get_d(zzmf_devinfo_t* pThis, const char* name, double* ret);
	ZZMF_EXPORT int ZZMF_API zzmf_devinfo_get_str(zzmf_devinfo_t* pThis, const char* name, char* ret, size_t* ret_size);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// device provider (mcast as backend)
	ZZMF_EXPORT zzmf_devprovider_t* ZZMF_API zzmf_devprovider_new();
	ZZMF_EXPORT void ZZMF_API zzmf_devprovider_add_ref(zzmf_devprovider_t* pThis);
	ZZMF_EXPORT void ZZMF_API zzmf_devprovider_release(zzmf_devprovider_t* pThis);

	ZZMF_EXPORT void ZZMF_API zzmf_devprovider_set_mcast_addr(zzmf_devprovider_t* pThis, const char* addr);
	ZZMF_EXPORT void ZZMF_API zzmf_devprovider_set_mcast_port(zzmf_devprovider_t* pThis, int port);
	ZZMF_EXPORT void ZZMF_API zzmf_devprovider_set_info(zzmf_devprovider_t* pThis, zzmf_devinfo_t* info); // { "id": "dev-id, e.g. 0x800006D0", "path": "uri-to-dev-service, e.g. /_dev0, /_dev1" }

	ZZMF_EXPORT void ZZMF_API zzmf_devprovider_init(zzmf_devprovider_t* pThis);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// session manager
	ZZMF_EXPORT zzmf_sessmgr_t* ZZMF_API zzmf_sessmgr_new();
	ZZMF_EXPORT void ZZMF_API zzmf_sessmgr_add_ref(zzmf_sessmgr_t* pThis);
	ZZMF_EXPORT void ZZMF_API zzmf_sessmgr_release(zzmf_sessmgr_t* pThis);

	ZZMF_EXPORT void ZZMF_API zzmf_sessmgr_set_auth(zzmf_sessmgr_t* pThis, const char* key);
	ZZMF_EXPORT void ZZMF_API zzmf_sessmgr_set_timeout_value(zzmf_sessmgr_t* pThis, int value); // in seconds
	ZZMF_EXPORT void ZZMF_API zzmf_sessmgr_find_by_id(zzmf_sessmgr_t* pThis, const char* sessid, zzmf_devinfo_t** ppInfo);

	ZZMF_EXPORT void ZZMF_API zzmf_sessmgr_init(zzmf_sessmgr_t* pThis);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// device service (fastcgi as backend)
	ZZMF_EXPORT zzmf_devservice_t* ZZMF_API zzmf_devservice_new();
	ZZMF_EXPORT void ZZMF_API zzmf_devservice_add_ref(zzmf_devservice_t* pThis);
	ZZMF_EXPORT void ZZMF_API zzmf_devservice_release(zzmf_devservice_t* pThis);

	ZZMF_EXPORT void ZZMF_API zzmf_devservice_set_path(zzmf_devservice_t* pThis, const char* path); // unix socket name e.g. "/tmp/fcgi.socket" or local socket e.g. ":port"
	ZZMF_EXPORT void ZZMF_API zzmf_devservice_set_backlog(zzmf_devservice_t* pThis, int backlog);
	ZZMF_EXPORT void ZZMF_API zzmf_devservice_set_group_info(zzmf_devservice_t* pThis, zzmf_devinfo_t* info);
	ZZMF_EXPORT void ZZMF_API zzmf_devservice_set_broker_endpoint(zzmf_devservice_t* pThis, const char* host, int port);
	ZZMF_EXPORT void ZZMF_API zzmf_devservice_set_sessmgr(zzmf_devservice_t* pThis, zzmf_sessmgr_t* pSessMgr);
	ZZMF_EXPORT void ZZMF_API zzmf_devservice_set_callbacks(zzmf_devservice_t* pThis, zzmf_devservice_on_request_t on_request, intptr_t user_data); // dynamic data from clients

	ZZMF_EXPORT void ZZMF_API zzmf_devservice_init(zzmf_devservice_t* pThis);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// device enumerator
	ZZMF_EXPORT zzmf_devenum_t* ZZMF_API zzmf_devenum_new();
	ZZMF_EXPORT void ZZMF_API zzmf_devenum_add_ref(zzmf_devenum_t* pThis);
	ZZMF_EXPORT void ZZMF_API zzmf_devenum_release(zzmf_devenum_t* pThis);

	ZZMF_EXPORT void ZZMF_API zzmf_devenum_set_mcast_addr(zzmf_devenum_t* pThis, const char* addr);
	ZZMF_EXPORT void ZZMF_API zzmf_devenum_set_mcast_port(zzmf_devenum_t* pThis, int port);
	ZZMF_EXPORT void ZZMF_API zzmf_devenum_set_callbacks(zzmf_devenum_t* pThis, zzmf_devenum_on_dev_found_t on_dev_found, intptr_t user_data);

	ZZMF_EXPORT void ZZMF_API zzmf_devenum_scan(zzmf_devenum_t* pThis, int interval); // scan devices (in ms)

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// device control
	ZZMF_EXPORT zzmf_devcon_t* ZZMF_API zzmf_devcon_new();
	ZZMF_EXPORT void ZZMF_API zzmf_devcon_add_ref(zzmf_devcon_t* pThis);
	ZZMF_EXPORT void ZZMF_API zzmf_devcon_release(zzmf_devcon_t* pThis);

	ZZMF_EXPORT void ZZMF_API zzmf_devcon_get_id(zzmf_devcon_t* pThis, uint32_t* id);
	ZZMF_EXPORT void ZZMF_API zzmf_devcon_get_mac_address(zzmf_devcon_t* pThis, uint8_t* ret, size_t* ret_size);
	ZZMF_EXPORT void ZZMF_API zzmf_devcon_get_ip(zzmf_devcon_t* pThis, char* ret, size_t* ret_size);
	ZZMF_EXPORT void ZZMF_API zzmf_devcon_get_port(zzmf_devcon_t* pThis, uint16_t* port);
	ZZMF_EXPORT void ZZMF_API zzmf_devcon_get_uri(zzmf_devcon_t* pThis, char* ret, size_t* ret_size);

	ZZMF_EXPORT void ZZMF_API zzmf_devcon_init(zzmf_devcon_t* pThis);

	ZZMF_EXPORT void ZZMF_API zzmf_devcon_request(zzmf_devcon_t* pThis, zzmf_devinfo_t* req, zzmf_devcon_on_response_t res, intptr_t user_data);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// video processing subsystem
	ZZMF_EXPORT zzmf_vpss_t* ZZMF_API zzmf_vpss_new();
	ZZMF_EXPORT void ZZMF_API zzmf_vpss_add_ref(zzmf_vpss_t* pThis);
	ZZMF_EXPORT void ZZMF_API zzmf_vpss_release(zzmf_vpss_t* pThis);

	ZZMF_EXPORT void ZZMF_API zzmf_vpss_begin(zzmf_vpss_t* pThis,
		int dst_w, int dst_h, uint32_t fourcc, uint8_t* dst[4], int dst_step[4]);
	ZZMF_EXPORT void ZZMF_API zzmf_vpss_end(zzmf_vpss_t* pThis);

	ZZMF_EXPORT void ZZMF_API zzmf_vpss_render_image(zzmf_vpss_t* pThis,
		int src_w, int src_h, uint32_t fourcc, uint8_t* src[4], int src_step[4],
		int src_crop_x, int src_crop_y, int src_crop_w, int src_crop_h,
		int dst_crop_x, int dst_crop_y, int dst_crop_w, int dst_crop_h);

#ifdef __cplusplus
}
#endif

#endif // __ZZMF_2817110C_3A76_41E7_B2F5_1EBC5BA75DA7__
