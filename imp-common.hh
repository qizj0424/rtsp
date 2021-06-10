/*
 * sample-common.h
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 */

#ifndef __SAMPLE_COMMON_HH__
#define __SAMPLE_COMMON_HH__

#include <imp/imp_common.h>
#include <imp/imp_osd.h>
#include <imp/imp_framesource.h>
#include <imp/imp_isp.h>
#include <imp/imp_encoder.h>
#include <unistd.h>

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* __cplusplus */

extern int g_Audio;
extern int g_Audio_Ns;
extern int g_Fps_Num;
extern int g_VideoWidth;
extern int g_VideoHeight;
extern int g_i2c_addr;
extern int g_wdr;
//extern int g_RcMode;
extern int g_BitRate;
extern int g_gop;
extern int g_adb;
extern int g_rndis;
extern int g_Speak;
extern int g_dmic;
extern int g_led;
extern int g_HV_Flip;
extern int g_QP;
extern int Mic_Volume;
extern int Spk_Volume;
extern int g_Dynamic_Fps;
extern int g_Power_save;
extern char g_Sensor_Name[16];


extern bool UVC_START_FLAG;
extern  IMPEncoderRcMode g_RcMode;

#define SENSOR_FRAME_RATE_NUM	25
#define SENSOR_FRAME_RATE_NUM_30	30
#define SENSOR_FRAME_RATE_DEN		1

#define SENSOR_GC2053
#define CHN0_EN                 1
#define CROP_EN					1

#if defined SENSOR_JXF23
#define SENSOR_NAME				"jxf23"
#define SENSOR_CUBS_TYPE        TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDR			0x40
#define SENSOR_WIDTH			1920
#define SENSOR_HEIGHT			1080
#define CHN0_EN                 1
#define CHN1_EN                 0
#define CHN2_EN                 0
#define CHN3_EN                 1
#define CROP_EN					0
#elif defined SENSOR_IMX335
#define SENSOR_NAME				"imx335"
#define SENSOR_CUBS_TYPE        TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDR			0x1a
#define SENSOR_WIDTH			2592
#define SENSOR_HEIGHT			1944
#define CHN0_EN                 1
#define CHN1_EN                 0
#define CHN2_EN                 0
#define CHN3_EN                 1
#define CROP_EN					0
#elif defined SENSOR_GC2053
#define SENSOR_NAME				"gc2053"
#define SENSOR_CUBS_TYPE        TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDR			0x37
#define SENSOR_WIDTH			1920
#define SENSOR_HEIGHT			1080
#define CHN0_EN                 1
#define CHN1_EN                 0
#define CHN2_EN                 0
#define CHN3_EN                 1
#define CROP_EN					0
#elif defined SENSOR_SC3335
#define SENSOR_NAME				"sc3335"
#define SENSOR_CUBS_TYPE        TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDR			0x30
#define SENSOR_WIDTH			2304
#define SENSOR_HEIGHT			1296
#define CHN0_EN                 1
#define CHN1_EN                 0
#define CHN2_EN                 0
#define CHN3_EN                 0
#define CROP_EN					0
#elif defined SENSOR_IMX307
#define SENSOR_NAME				"imx307"
#define SENSOR_CUBS_TYPE        TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDR			0x1a
#define SENSOR_WIDTH			1920
#define SENSOR_HEIGHT			1080
#define CHN0_EN                 1
#define CHN1_EN                 0
#define CHN2_EN                 0
#define CHN3_EN                 0
#define CROP_EN					0
#elif defined SENSOR_IMX327
#define SENSOR_NAME				"imx327"
#define SENSOR_CUBS_TYPE        TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDR			0x36
#define SENSOR_WIDTH			1920
#define SENSOR_HEIGHT			1080
#define CHN0_EN                 1
#define CHN1_EN                 0
#define CHN2_EN                 0
#define CHN3_EN                 0
#define CROP_EN					0
#endif

#define SENSOR_WIDTH_SECOND		640
#define SENSOR_HEIGHT_SECOND	360

#define SENSOR_WIDTH_THIRD		1280
#define SENSOR_HEIGHT_THIRD		720

#define BITRATE_720P_Kbs        1000

#define NR_FRAMES_TO_SAVE		200
#define STREAM_BUFFER_SIZE		(1 * 1024 * 1024)

#define ENC_VIDEO_CHANNEL		0
#define ENC_JPEG_CHANNEL		1

#define STREAM_FILE_PATH_PREFIX		"/tmp"
#define SNAP_FILE_PATH_PREFIX		"/tmp"

#define UVC_LICENSE_PATH_PREFIX		"/tmp/uvc_license"

#define OSD_REGION_WIDTH		16
#define OSD_REGION_HEIGHT		34
#define OSD_REGION_WIDTH_SEC		8
#define OSD_REGION_HEIGHT_SEC   	18


#define SLEEP_TIME			1

#define FS_CHN_NUM			1  //MIN 1,MAX 3
#define IVS_CHN_ID          2

#define CH0_INDEX  0
#define CH1_INDEX  1
#define CH2_INDEX  2
#define CH3_INDEX  3
#define CHN_ENABLE 1
#define CHN_DISABLE 0

/*#define SUPPORT_RGB555LE*/

struct chn_conf{
	unsigned int index;//0 for main channel ,1 for second channel
	unsigned int enable;
  	IMPEncoderProfile payloadType;
	IMPFSChnAttr fs_chn_attr;
	IMPCell framesource_chn;
	IMPCell imp_encoder;
};

#define  CHN_NUM  ARRAY_SIZE(chn)

#define LED_GPIO 53

#define UVC_BUF_NUM 	2
#define UVC_BUF_SIZE	1.5*1024*1024


int sample_system_init();
int sample_system_exit();

int sample_framesource_streamon();
int sample_framesource_streamoff();

int sample_framesource_init();
int sample_framesource_exit();

int sample_encoder_init();
int sample_jpeg_init();
int sample_jpeg_exit(void);
int sample_encoder_exit(void);


int sample_get_frame();
int sample_get_jpeg_snap(char *img_buf);
int sample_get_h264_snap(char *img_buf);
int sample_get_yuv_snap(char *img_buf);
int ucamera_uvc_pu_attr_save(int type, int value);

#ifdef T31
void sample_audio_dmic_init(void);
void sample_audio_dmic_exit(void);
int sample_audio_dmic_pcm_get(short *pcm);
#endif

void sample_audio_amic_init(void);
void sample_audio_amic_exit(void);
int sample_audio_amic_pcm_get(short *pcm);

int sample_get_h264_start(void);
int sample_get_h264_stop(void);

int imp_sdk_init(int format, int width, int height);
int imp_sdk_deinit(int format);
int imp_system_init(void);
int imp_system_exit(void);

int sample_set_mic_volume(int vol);
int sample_set_spk_volume(int vol);
int sample_set_spk_mute(int mute);
int sample_set_mic_mute(int mute);
int sample_get_record_off();
int sample_get_record_on();
int sample_get_speak_off();
int sample_get_speak_on();
int sample_audio_play_start(void);

int sample_ucamera_led_init(int gpio);
int sample_ucamera_led_ctl(int gpio, int value);
int speak_volume_write_config(int val);

//int imp_config(void);
int uvc_system_init(void);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
#endif /* __SAMPLE_COMMON_H__ */
