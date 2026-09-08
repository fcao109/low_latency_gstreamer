#include <stdio.h>
#include "qcap.h"
#include "qcap.windef.h"
#include "qcap.linux.h"
#include "videoPipelineApi.h"
#include "server.h"

void *yuan_serverData;

typedef void * PVOID;

int type_input;

int create_device(int devnum, PVOID pDevice);
int destory_device(PVOID pDevice);

PVOID m_device_left;
PVOID m_device_right;

int frameCnt = 0;

unsigned char* frame_left;
unsigned char* frame_right;

ULONG m_nVideoWidth;
ULONG m_nVideoHeight;
double m_dVideoFrameRate;
ULONG m_nAudioChannels;
ULONG m_nAudioBitsPerSample;
ULONG m_nAudioSampleFrequency;



int copy_buffer;
int copy_audio_buffer;

static QRETURN on_process_signal_removed( PVOID pDevice, ULONG nVideoInput, ULONG nAudioInput, PVOID pUserData )
{
    printf("signal removed\n\n");

    return QCAP_RT_OK;
}

static QRETURN on_process_format_changed( PVOID pDevice, ULONG nVideoInput, ULONG nAudioInput, ULONG nVideoWidth, ULONG nVideoHeight, BOOL bVideoIsInterleaved, double dVideoFrameRate, ULONG nAudioChannels, ULONG nAudioBitsPerSample,  ULONG nAudioSampleFrequency, PVOID pUserData )
{
	ULONG i = (uintptr_t)pUserData;

    m_nVideoWidth = nVideoWidth;
    m_nVideoHeight = nVideoHeight;
    m_dVideoFrameRate = dVideoFrameRate;
    m_nAudioChannels = nAudioChannels;
    m_nAudioBitsPerSample = nAudioBitsPerSample;
	m_nAudioSampleFrequency = nAudioSampleFrequency;

    printf("format_changed\n\n");

    return QCAP_RT_OK;
}

static QRETURN on_process_video_preview_left( PVOID pDevice, double dSampleTime, BYTE * pFrameBuffer, ULONG nFrameBufferLen, PVOID pUserData )
{
	ULONG i = (uintptr_t)pUserData;

	if(i == 0)
	{
		PVOID pRCBuffer = QCAP_BUFFER_GET_RCBUFFER(pFrameBuffer, nFrameBufferLen);
    	qcap_av_frame_t* pAVFrame = (qcap_av_frame_t*)QCAP_RCBUFFER_LOCK_DATA(pRCBuffer);

	memcpy(frame_left, pAVFrame->pData[0], pAVFrame->nPitch[0] * m_nVideoHeight);
	memcpy(frame_left + pAVFrame->nPitch[0] * m_nVideoHeight, pAVFrame->pData[1], pAVFrame->nPitch[1] * m_nVideoHeight/2);

	frameCnt++;
	// logMessage("frameCnt=" + std::to_string(frameCnt));
	((ServerData *)yuan_serverData)->capture_frame_cnt = frameCnt;
	((ServerData *)yuan_serverData)->frameQueue->push(frame_left);

    	QCAP_RCBUFFER_UNLOCK_DATA(pRCBuffer);
	}

    return QCAP_RT_OK;

}

static QRETURN on_process_video_preview_right( PVOID pDevice, double dSampleTime, BYTE * pFrameBuffer, ULONG nFrameBufferLen, PVOID pUserData )
{
	ULONG i = (uintptr_t)pUserData;

	if(i == 0)
	{
		PVOID pRCBuffer = QCAP_BUFFER_GET_RCBUFFER(pFrameBuffer, nFrameBufferLen);
    	qcap_av_frame_t* pAVFrame = (qcap_av_frame_t*)QCAP_RCBUFFER_LOCK_DATA(pRCBuffer);
#if 0
    	//get channel 0 buffer
    	if(copy_buffer)
    	{
    		printf("\nframe: %p %p \n", pAVFrame->pData[0], pAVFrame->pData[1]);
    		printf("\n nPitch: %d %d \n", pAVFrame->nPitch[0], pAVFrame->nPitch[1]);
    		FILE* fp = fopen("video.nv12", "wb");
    		fwrite(pAVFrame->pData[0], 1, pAVFrame->nPitch[0] * m_nVideoHeight, fp);
    		fwrite(pAVFrame->pData[1], 1, pAVFrame->nPitch[1] * m_nVideoHeight/2, fp);
    		fclose(fp);
    		copy_buffer = 0;
    	}
    	//copy channel 0 buffer
#else
	memcpy(frame_left, pAVFrame->pData[0], pAVFrame->nPitch[0] * m_nVideoHeight);
	memcpy(frame_left + pAVFrame->nPitch[0] * m_nVideoHeight, pAVFrame->pData[1], pAVFrame->nPitch[1] * m_nVideoHeight/2);

	frameCnt++;
	// logMessage("frameCnt=" + std::to_string(frameCnt));
	((ServerData *)yuan_serverData)->capture_frame_cnt = frameCnt;
	((ServerData *)yuan_serverData)->frameQueue->push(frame_left);

#endif
    	QCAP_RCBUFFER_UNLOCK_DATA(pRCBuffer);
	}

    return QCAP_RT_OK;

}

void print_message()
{
	printf( "\nTYPE COMMAND------------------------------------------ \n\n" );
	printf( "1: Create device \n" );
	printf( "2: Save a channel 0 video frame \n" );
	printf( "3: Destory device \n" );
	printf( "0: Exit \n" );
	printf( "\nCommand:");
}

void yuan_main()
{
	m_device_left = NULL;
	m_device_right = NULL;

	m_nVideoWidth = 0;
	m_nVideoHeight = 0;
	m_dVideoFrameRate = 0.0;
	m_nAudioChannels = 0;
	m_nAudioBitsPerSample = 0;
	m_nAudioSampleFrequency = 0;

	create_device(0, &m_device_left);
	// create_device(1, &m_device_right);

	copy_buffer = 1;

	frame_left = (unsigned char*)malloc(1920 * 1080 * 3 / 2);
	frame_right = (unsigned char*)malloc(1920 * 1080 * 3 / 2);

	while(true)
	{
#if 0
		print_message();

		scanf("%d", &type_input);

		if(type_input == 0)break;

		switch(type_input)
		{
			case 1: create_device();
			break;

			case 2: copy_buffer = 1;
			break;

			case 3:	destory_device();
			break;

			default: printf( "\nerror command-----------------------------------\n");;
		}
#else
		usleep(30000);
#endif
	}

	destory_device(m_device_left);
	// destory_device(m_device_right);

    return;

}

int create_device(int devnum, PVOID pDevice)
{

    QRESULT ret = QCAP_CREATE( "SC0720 PCI", devnum, NULL, &pDevice, TRUE, TRUE);

    if(ret != 0)
    {
        printf("QCAP Create Failed\n\n");
        return -1;
    }

    QCAP_REGISTER_SIGNAL_REMOVED_CALLBACK(  pDevice, on_process_signal_removed, (PVOID)0 );

    QCAP_REGISTER_FORMAT_CHANGED_CALLBACK(  pDevice, on_process_format_changed, (PVOID)0 );

	(devnum == 0) ?
		QCAP_REGISTER_VIDEO_PREVIEW_CALLBACK(  pDevice, on_process_video_preview_left, (PVOID)0 ) :
		QCAP_REGISTER_VIDEO_PREVIEW_CALLBACK(  pDevice, on_process_video_preview_right, (PVOID)0 );

    QCAP_SET_VIDEO_DEFAULT_OUTPUT_FORMAT(  pDevice, QCAP_COLORSPACE_TYPE_NV12, 0, 0, 0, 0);

	QCAP_SET_VIDEO_INPUT(  pDevice, QCAP_INPUT_TYPE_SDI );

	QCAP_SET_AUDIO_INPUT(  pDevice, QCAP_INPUT_TYPE_EMBEDDED_AUDIO );

	QCAP_RUN( pDevice);

    return 0;

}

int destory_device(PVOID pDevice)
{

	QCAP_STOP( pDevice);

	QCAP_DESTROY( pDevice);

	pDevice = NULL;


    return 0;
}