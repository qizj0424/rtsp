/*
 * sample-Encoder-jpeg.c
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 */

#include <pthread.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include <semaphore.h>
#include <sys/ioctl.h>

#include <imp/imp_log.h>
#include <imp/imp_common.h>
#include <imp/imp_system.h>
#include <imp/imp_framesource.h>
#include <imp/imp_encoder.h>
#include <usbcamera.h>
#include "VideoInput.hh"
//#include <AntiCopy_Verify.h>
#include "imp-common.hh"

#define TAG "Sample-UCamera"
#define LABEL_LEN                64
#define UCAMERA_DEBUG           0

#if UCAMERA_DEBUG
#define Ucamera_DEBUG(format, arg...)                                            \
	printf("%s: " format "\n" , "[Ucamera]", ## arg)
#else
#define Ucamera_DEBUG(format, arg...)
#endif

#ifndef T20
#define MOTOR_MOVE_DELAY_TIME   (1000 * 1000)
#define MOTOR_MOVE              _IOW('M', 1, int)
static int focus_trigger_value = 30;
static int motor_pixel = 200;
static int af_on_off = 0;
static int motor_step = 50;
static int motor_sleep_time = 80 * 1000;

int dac_value = 0;
static int dac_pos = 100;
static int dac_pos_old = 100;
static int flag_refocus = 0;
static int ev_last;

sem_t ucam_Af_sem;
pthread_mutex_t uvc_Af_mutex;
int flag_af_check;
#endif

static char manufact_label[LABEL_LEN] = "HD Web Camera";
static char product_label[LABEL_LEN] = "HD Web Camera";
static char serial_label[LABEL_LEN] = "Ucamera001";
static char video_device_name[LABEL_LEN] = "HD Web Camera";
static char audio_device_name[LABEL_LEN] = "Mic-HD Web Ucamera";

static int  VENDOR_ID = 0x05a3;
static int  PRODUCT_ID = 0x9331;
static int  DEVICE_BCD = 0x0105;

static int spk_volume_line = 0;

static int Ucam_Stream_On = 0;
extern struct chn_conf chn[];
#ifdef T31
static IMPISPFrontCrop fcrop_obj;
#endif

FILE *uvc_attr_fd = NULL;
FILE *uvc_config_fd = NULL;
pthread_mutex_t uvc_attr_mutex;
sem_t ucam_ready_sem;
#ifndef T20
static int imp_inited = 0;
#endif

struct uvc_pu_string {
	char id;
	const char *s;
};


static int strToInt(char const* str) {
	int val;
	if (sscanf(str, "%d", &val) == 1) return val;
	return -1;
}

static int strToInt_Hex(char const* str) {
	int val;
	if (sscanf(str, "%x", &val) == 1) return val;
	return -1;
}

static int Dec_to_Hex(int const dec) {
	char str[10] = {0};
	int val = 0;
	sprintf(str, "%x", dec);
	if (sscanf(str, "%x", &val) == 1) return val;
	return -1;
}

int htoi(char s[])
{
	int n = 0;
	int i = 0;
	while (s[i] != '\0' && s[i] != '\n') {
		if (s[i] == '0') {
			if (s[i+1] == 'x' || s[i+1] == 'X')
				i+=2;
		}
		if (s[i] >= '0' && s[i] <= '9') {
			n = n * 16 + (s[i] - '0');
		} else if (s[i] >= 'a' && s[i] <= 'f') {
			n = n * 16 + (s[i] - 'a') + 10;
		} else if (s[i] >= 'A' && s[i] <= 'F') {
			n = n * 16 + (s[i] - 'A') + 10;
		} else
			return -1;
		++i;
	}

	return n;
}

int imp_system_init(void)
{
	int ret;
	/* Step.1 System init */
	ret = sample_system_init();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_System_Init() failed\n");
		return -1;
	}
	return 0;
}

int imp_sdk_init(int format, int width, int height)
{
	int i, ret;
	IMPFSChnAttr *imp_chn_attr_tmp;
	Ucamera_LOG("IMP SDK reinit!\n");

	imp_chn_attr_tmp = &chn[0].fs_chn_attr;

	/* set base info */
	imp_chn_attr_tmp->outFrmRateNum = g_Fps_Num;
	imp_chn_attr_tmp->picWidth = width;
	imp_chn_attr_tmp->picHeight = height;
	imp_chn_attr_tmp->nrVBs = 2;

	/* PS : resolution must be standard 16:9 or 4:3 */
#ifdef T31
	/* T21/T31 : scaler, then crop */
	if (g_VideoWidth == g_VideoHeight * 4 / 3) {
		/* sensor setting - width : height = 4 : 3 */
		/*zoom need to open the scaler*/
		imp_chn_attr_tmp->scaler.enable    = 1;
		imp_chn_attr_tmp->scaler.outwidth  = width;
		imp_chn_attr_tmp->scaler.outheight = width * 3 / 4;

		/* set crop attr */
		if (width == (height * 16 / 9)) {
			imp_chn_attr_tmp->crop.enable = 1;
			imp_chn_attr_tmp->crop.left   = 0;
			imp_chn_attr_tmp->crop.top    = height / 6;
			imp_chn_attr_tmp->crop.width  = width;
			imp_chn_attr_tmp->crop.height = height;
		} else {
			imp_chn_attr_tmp->crop.enable = 0;
		}
	} else {
		/* sensor setting - width : height = 16 : 9 */
		/*zoom need to open the scaler*/
		imp_chn_attr_tmp->scaler.enable    = 1;
		imp_chn_attr_tmp->scaler.outwidth  = height * 16 / 9;
		imp_chn_attr_tmp->scaler.outheight = height;

		/* set crop attr */
		if (width == (height * 4 / 3)) {
			imp_chn_attr_tmp->crop.enable = 1;
			imp_chn_attr_tmp->crop.left   = width / 6;
			imp_chn_attr_tmp->crop.top    = 0;
			imp_chn_attr_tmp->crop.width  = width;
			imp_chn_attr_tmp->crop.height = height;
		} else {
			imp_chn_attr_tmp->crop.enable = 0;
		}
	}
#elif defined T21
	/* T21/T31 : scaler, then crop */
	if (g_VideoWidth == g_VideoHeight * 4 / 3) {
		if(width != g_VideoWidth){
		/* sensor setting - width : height = 4 : 3 */
			imp_chn_attr_tmp->scaler.enable    = 1;
			imp_chn_attr_tmp->scaler.outwidth  = width;
			imp_chn_attr_tmp->scaler.outheight = width * 3 / 4;
		} else {
			imp_chn_attr_tmp->scaler.enable    = 0;
		}

		/* set crop attr */
		if (width == (height * 16 / 9)) {
			imp_chn_attr_tmp->crop.enable = 1;
			imp_chn_attr_tmp->crop.left   = 0;
			imp_chn_attr_tmp->crop.top    = height / 6;
			imp_chn_attr_tmp->crop.width  = width;
			imp_chn_attr_tmp->crop.height = height;
		} else {
			imp_chn_attr_tmp->crop.enable = 0;
		}
	} else {
		/* sensor setting - width : height = 16 : 9 */
		/*zoom need to open the scaler*/
		if(width != g_VideoWidth){
			imp_chn_attr_tmp->scaler.enable    = 1;
			imp_chn_attr_tmp->scaler.outwidth  = height * 16 / 9;
			imp_chn_attr_tmp->scaler.outheight = height;
		} else {
			imp_chn_attr_tmp->scaler.enable    = 0;
		}

		/* set crop attr */
		if (width == (height * 4 / 3)) {
			imp_chn_attr_tmp->crop.enable = 1;
			imp_chn_attr_tmp->crop.left   = width / 6;
			imp_chn_attr_tmp->crop.top    = 0;
			imp_chn_attr_tmp->crop.width  = width;
			imp_chn_attr_tmp->crop.height = height;
		} else {
			imp_chn_attr_tmp->crop.enable = 0;
		}
	}
#elif defined T20
	/* T20 : crop, then scaler */
	if (g_VideoWidth == g_VideoHeight * 4 / 3) {
		/* sensor setting - width : height = 4 : 3 */
		/* set crop attr */
		if (width == (height * 16 / 9)) {
			imp_chn_attr_tmp->crop.enable = 1;
			imp_chn_attr_tmp->crop.left   = 0;
			imp_chn_attr_tmp->crop.top    = g_VideoHeight / 8;
			imp_chn_attr_tmp->crop.width  = g_VideoWidth;
			imp_chn_attr_tmp->crop.height = g_VideoHeight * 3 / 4;
		} else {
			imp_chn_attr_tmp->crop.enable = 0;
		}

		/* set scaler attr */
		if (width != g_VideoWidth) {
			imp_chn_attr_tmp->scaler.enable    = 1;
			imp_chn_attr_tmp->scaler.outwidth  = width;
			imp_chn_attr_tmp->scaler.outheight = height;
		} else {
			imp_chn_attr_tmp->scaler.enable = 0;
		}
	} else {
		/* sensor setting - width : height = 16 : 9 */
		/* set crop attr */
		if (width == (height * 4 / 3)) {
			imp_chn_attr_tmp->crop.enable = 1;
			imp_chn_attr_tmp->crop.left   = g_VideoWidth / 8;
			imp_chn_attr_tmp->crop.top    = 0;
			imp_chn_attr_tmp->crop.width  = g_VideoWidth * 3 / 4;
			imp_chn_attr_tmp->crop.height = g_VideoHeight;
		} else {
			imp_chn_attr_tmp->crop.enable = 0;
		}

		/* set scaler attr */
		if (height != g_VideoHeight) {
			imp_chn_attr_tmp->scaler.enable    = 1;
			imp_chn_attr_tmp->scaler.outwidth  = width;
			imp_chn_attr_tmp->scaler.outheight = height;

		} else {
			imp_chn_attr_tmp->scaler.enable = 0;
		}
	}
#endif

	/* correct crop,scaler,top,left value to match soc platform */
#ifdef T31
	/* T31: width(scaler) 2 */
	if (imp_chn_attr_tmp->scaler.enable) {
		if (imp_chn_attr_tmp->scaler.outwidth % 2)
			imp_chn_attr_tmp->scaler.outwidth--;
	}
#elif defined T21
	/* T21: width 16, height 8, top 2, left 2 */
	if (imp_chn_attr_tmp->scaler.enable) {
		if (imp_chn_attr_tmp->scaler.outwidth % 16)
			imp_chn_attr_tmp->scaler.outwidth -= imp_chn_attr_tmp->scaler.outwidth % 16;
		if (imp_chn_attr_tmp->scaler.outheight % 8)
			imp_chn_attr_tmp->scaler.outheight -= imp_chn_attr_tmp->scaler.outheight % 8;
	}

	if (imp_chn_attr_tmp->crop.enable) {
		if (imp_chn_attr_tmp->crop.top % 2)
			imp_chn_attr_tmp->crop.top--;
		if (imp_chn_attr_tmp->crop.left % 2)
			imp_chn_attr_tmp->crop.left--;
		if (imp_chn_attr_tmp->crop.width % 16)
			imp_chn_attr_tmp->crop.width -= imp_chn_attr_tmp->crop.width % 16;
		if (imp_chn_attr_tmp->crop.height % 8)
			imp_chn_attr_tmp->crop.height -= imp_chn_attr_tmp->crop.height % 16;
	}
#elif defined T20
	/* no need correct */
#endif

	if (imp_chn_attr_tmp->crop.enable)
		Ucamera_LOG("IMP: Crop enable w:%d h:%d left:%d top:%d\n",
				imp_chn_attr_tmp->crop.width,
				imp_chn_attr_tmp->crop.height,
				imp_chn_attr_tmp->crop.left,
				imp_chn_attr_tmp->crop.top);

	if (imp_chn_attr_tmp->scaler.enable)
		Ucamera_LOG("IMP: Scaler enable w:%d h:%d\n",
				imp_chn_attr_tmp->scaler.outwidth,
				imp_chn_attr_tmp->scaler.outheight);

	//if (g_Power_save) {
	//	ret = IMP_ISP_EnableSensor();
	//	if(ret < 0){
	//		IMP_LOG_ERR(TAG, "failed to EnableSensor\n");
	//		return -1;
	//	}
	//	imp_isp_tuning_init();
	//}


	/* Step.2 FrameSource init */
	ret = sample_framesource_init();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "FrameSource init failed\n");
		return -1;
	}

	if (format == V4L2_PIX_FMT_YUYV || format == V4L2_PIX_FMT_NV12) {
		IMPFSChnAttr fs_chn_attr[2];
		/* Step.3 Snap raw config */
		ret = IMP_FrameSource_GetChnAttr(0, &fs_chn_attr[0]);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "%s(%d):IMP_FrameSource_GetChnAttr failed\n", __func__, __LINE__);
			return -1;
		}

		fs_chn_attr[0].pixFmt = PIX_FMT_NV12;//PIX_FMT_YUYV422;
		ret = IMP_FrameSource_SetChnAttr(0, &fs_chn_attr[0]);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "%s(%d):IMP_FrameSource_SetChnAttr failed\n", __func__, __LINE__);
			return -1;
		}

		/* Step.3 config sensor reg to output colrbar raw data*/
		/* to do */

		/* Step.4 Stream On */
		if (chn[0].enable){
			ret = IMP_FrameSource_EnableChn(chn[0].index);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_FrameSource_EnableChn(%d) error: %d\n", ret, chn[0].index);
				return -1;
			}
		}

		/* Step.4 Snap raw */
		ret = IMP_FrameSource_SetFrameDepth(0, 1);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "%s(%d):IMP_FrameSource_SetFrameDepth failed\n", __func__, __LINE__);
			return -1;
		}
		return 0;
	}

	for (i = 0; i < FS_CHN_NUM; i++) {
		if (chn[i].enable) {
			ret = IMP_Encoder_CreateGroup(chn[i].index);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_Encoder_CreateGroup(%d) error !\n", i);
				return -1;
			}
		}
	}

	/* Step.3 Encoder init */
	switch (format) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_NV12:
		break;
	case V4L2_PIX_FMT_MJPEG:
		ret = sample_jpeg_init();
		break;
	case V4L2_PIX_FMT_H264:
		ret = sample_encoder_init();
		break;
	}
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "Encoder init failed\n");
		return -1;
	}

	/* Step.4 Bind */
	for (i = 0; i < FS_CHN_NUM; i++) {
		if (chn[i].enable) {
			ret = IMP_System_Bind(&chn[i].framesource_chn, &chn[i].imp_encoder);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "Bind FrameSource channel%d and Encoder failed\n",i);
				return -1;
			}
		}
	}

	/* Step.5 Stream On */
	ret = sample_framesource_streamon();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "ImpStreamOn failed\n");
		return -1;
	}

	if (format == V4L2_PIX_FMT_H264) {
	#if 1
		ret = IMP_Encoder_StartRecvPic(chn[0].index);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "IMP_Encoder_StartRecvPic(%d) failed\n", chn[0].index);
			return -1;
		}
	#else
		sample_get_h264_start();
	#endif
	}

	return 0;
}

int imp_sdk_deinit(int format)
{
	int i, ret;

	if (format == V4L2_PIX_FMT_YUYV || format == V4L2_PIX_FMT_NV12) {
		/* Step.5 Stream Off */
		if (chn[0].enable){
			ret = IMP_FrameSource_DisableChn(chn[0].index+3);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_FrameSource_DisableChn(%d) error: %d\n", ret, chn[0].index);
				return -1;
			}
		}

		/* Step.6 FrameSource exit */
		if (chn[0].enable) {
			/*Destroy channel i*/
			//ret = IMP_FrameSource_DestroyChn(0);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_FrameSource_DestroyChn() error: %d\n", ret);
				return -1;
			}
		}
	} else {

		if (format == V4L2_PIX_FMT_H264) {
		#if 1
			ret = IMP_Encoder_StopRecvPic(chn[0].index+3);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_Encoder_StopRecvPic(%d) failed\n", chn[0].index);
				return -1;
			}
		#else
			sample_get_h264_stop();
		#endif
		}

		/* Step.a Stream Off */
		ret = sample_framesource_streamoff();
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "FrameSource StreamOff failed\n");
			return -1;
		}

		/* Step.b UnBind */
		for (i = 0; i < FS_CHN_NUM; i++) {
			if (chn[i].enable) {
				//ret = IMP_System_UnBind(&chn[i].framesource_chn, &chn[i].imp_encoder);
				//if (ret < 0) {
				//	IMP_LOG_ERR(TAG, "UnBind FrameSource channel%d and Encoder failed\n",i);
				//	return -1;
				//}
			}
		}

		/* Step.c Encoder exit */
		switch (format) {
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_NV12:
			break;
		case V4L2_PIX_FMT_MJPEG:
			ret = sample_jpeg_exit();
			break;
		case V4L2_PIX_FMT_H264:
			//ret = sample_encoder_exit();
			break;
		}
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "Encoder init failed\n");
			return -1;
		}

		for (i = 0; i < FS_CHN_NUM; i++) {
			if (chn[i].enable) {
				ret = IMP_Encoder_DestroyGroup(chn[i].index+3);
				if (ret < 0) {
					IMP_LOG_ERR(TAG, "IMP_Encoder_CreateGroup(%d) error !\n", i);
					return -1; }
			}
		}

		/* Step.d FrameSource exit */
		ret = sample_framesource_exit();
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "FrameSource exit failed\n");
			return -1;
		}
	}

	if (g_Power_save) {
		//ret = IMP_ISP_DisableTuning();
		//if(ret < 0){
		//	IMP_LOG_ERR(TAG, "IMP_ISP_DisableTuning failed\n");
		//	return -1;
		//}

		//ret = IMP_ISP_DisableSensor();
		//if(ret < 0){
		//	IMP_LOG_ERR(TAG, "failed to EnableSensor\n");
		//	return -1;
		//}
	}

	return 0;
}

int imp_system_exit(void)
{
	int ret;
	/* Step.e System exit */
	ret = sample_system_exit();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "sample_system_exit() failed\n");
	}
	return ret;
}

int imp_SensorFPS_Adapture(void)
{
	int ret, ev_cur = 0;
	int sensor_FPS = 0;
	IMPISPEVAttr evattr = {0};

	if (Ucam_Stream_On == 0)
		return 0;

	ret = IMP_ISP_Tuning_GetEVAttr(&evattr);
	if (ret < 0) {
		Ucamera_LOG("failed to get evattr\n");
		return -1;
	}
	Ucamera_LOG("IMP get Ev:%d\n", evattr.ev);

	if (evattr.ev > 8000)
		ev_cur = 2;
	else if (evattr.ev > 4000)
		ev_cur = 1;
	else
		ev_cur = 0;

	if (ev_cur == ev_last)
		return 0;

	ev_last = ev_cur;

	switch (ev_cur) {
	case 2:
		sensor_FPS = 12;
		break;
	case 1:
		sensor_FPS = 15;
		break;
	case 0:
		sensor_FPS = g_Fps_Num;
		break;
	default:
		return 0;
	}

	ret = IMP_ISP_Tuning_SetSensorFPS(sensor_FPS, 1);
	if (ret < 0) {
		Ucamera_LOG("failed to set sensor fps\n");
		return -1;
	}

	return 0;
}


int ucamera_load_config(struct Ucamera_Cfg *ucfg )
{
	int nframes = 0;
	unsigned int i, width, height;
	int line_num = 0;
	char key[64] = {0};
	char value[64] = {0};
	char *line_str = NULL;
	struct Ucamera_Video_Cfg *vcfg;
	int intervals = 10000000/30;

	vcfg = &(ucfg->vcfg);
	if ((uvc_config_fd = fopen("/system/config/uvc.config", "r+")) == NULL) {
		Ucamera_LOG("open config file failed %s!\n", __func__);
		return -1;
	}
	line_str =(char*)malloc(256*sizeof(char));
	if(line_str == NULL){
		Ucamera_LOG("malloc if line_str is failed\n");
		return -1;
	}

	/* printf("\n**********Config Param**********\n"); */
	while (!feof(uvc_config_fd)) {
		if (fscanf(uvc_config_fd, "%[^\n]", line_str) < 0)
			break;
		fseek(uvc_config_fd , 1, SEEK_CUR);
		line_num++;

		if (sscanf(line_str, "%[^:]:%[^\n]", key, value) != 2) {
			printf("warning: skip config %s\n", line_str);
			fseek(uvc_config_fd , 1, SEEK_CUR);
			continue;
		}

		char *ch = strchr(key, ' ');
		if (ch) *ch = 0;
		if (strcmp(key, "sensor_name") == 0) {
			/* printf("%s %s\n", key, value); */
			strncpy(g_Sensor_Name, value, sizeof(g_Sensor_Name));
#ifndef T20
		} else if(strcmp(key, "focus_trigger_value") == 0){
			focus_trigger_value = strToInt(value);
		} else if(strcmp(key,"motor_pixel") == 0) {
			motor_pixel = strToInt(value);
		} else if (strcmp(key, "af_on_off") == 0) {
			af_on_off = strToInt(value);
		} else if (strcmp(key, "motor_step") == 0) {
			motor_step = strToInt(value);
		} else if (strcmp(key, "motor_sleep") == 0) {
			motor_sleep_time = strToInt(value);
#endif
		} else if (strcmp(key, "i2c_addr") == 0) {
			/* printf("%s %s\n", key, value); */
			g_i2c_addr = htoi(value);
		} else if (strcmp(key, "fps_num") == 0) {
			/* printf("%s %s\n", key, value); */
			g_Fps_Num = strToInt(value);
		} else if (strcmp(key, "focus_max") == 0) {
			/* printf("%s %s\n", key, value); */
			//focus.data[UVC_MAX] = strToInt(value);
		} else if (strcmp(key, "width") == 0) {
			/* printf("%s %s\n", key, value); */
			g_VideoWidth = strToInt(value);
		} else if (strcmp(key, "height") == 0) {
			/* printf("%s %s\n", key, value); */
			g_VideoHeight = strToInt(value);
		} else if (strcmp(key, "wdr_en") == 0) {
			/* printf("%s %s\n", key, value); */
			g_wdr = strToInt(value);
		} else if (strcmp(key, "bitrate") == 0) {
			/* printf("%s %s\n", key, value); */
			g_BitRate = strToInt(value);
		} else if (strcmp(key, "audio_en") == 0) {
			/* printf("%s %s\n", key, value); */
			g_Audio = strToInt(value);
			ucfg->audio_en = g_Audio;
		} else if (strcmp(key, "gop") == 0) {
			/* printf("%s %s\n", key, value); */
			g_gop = strToInt(value);
		} else if (strcmp(key, "qp_value") == 0) {
			/* printf("%s %s\n", key, value); */
			g_QP = strToInt(value);
		} else if (strcmp(key, "adb_en") == 0) {
			/* printf("%s %s\n", key, value); */
			g_adb = strToInt(value);
			ucfg->adb_en = g_adb;
		} else if (strcmp(key, "stillcap_en") == 0) {
			/* printf("%s %s\n", key, value); */
			ucfg->stillcap = strToInt(value);
		} else if (strcmp(key, "rndis_en") == 0) {
			/* printf("%s %s\n", key, value); */
			g_rndis = strToInt(value);
		} else if (strcmp(key, "dmic_en") == 0) {
			/* printf("%s %s\n", key, value); */
			g_dmic = strToInt(value);
		} else if (strcmp(key, "speak_en") == 0) {
			/* printf("%s %s\n", key, value); */
			g_Speak = strToInt(value);
		} else if (strcmp(key, "h264_en") == 0) {
			/* printf("%s %s\n", key, value); */
			ucfg->h264_en = strToInt(value);
		} else if (strcmp(key, "uvc_led") == 0) {
			/* printf("%s %s\n", key, value); */
			g_led = strToInt(value);
		} else if (strcmp(key, "mic_volume") == 0) {
			/* printf("%s %s\n", key, value); */
			Mic_Volume = strToInt(value);
		} else if (strcmp(key, "speak_volume") == 0) {
			/* printf("%s %s\n", key, value); */
			Spk_Volume = strToInt(value);
			spk_volume_line = line_num;
		} else if (strcmp(key, "audio_ns") == 0) {
			/* printf("%s %s\n", key, value); */
			g_Audio_Ns = strToInt(value);
		} else if (strcmp(key, "hvflip") == 0) {
			/* printf("%s %s\n", key, value); */
			g_HV_Flip = strToInt(value);
		} else if (strcmp(key, "dynamic_fps") == 0) {
			/* printf("%s %s\n", key, value); */
			g_Dynamic_Fps = strToInt(value);
		} else if (strcmp(key, "device_bcd") == 0) {
			/* printf("%s %s\n", key, value); */
			DEVICE_BCD = strToInt_Hex(value);
		} else if (strcmp(key, "product_id") == 0) {
			/* printf("%s %s\n", key, value); */
			PRODUCT_ID = strToInt_Hex(value);
		} else if (strcmp(key, "vendor_id") == 0) {
			/* printf("%s %s\n", key, value); */
			VENDOR_ID = strToInt_Hex(value);
		} else if (strcmp(key, "serial_lab") == 0) {
			/* printf("%s %s\n", key, value); */
			strncpy(serial_label, value, LABEL_LEN);
		} else if (strcmp(key, "product_lab") == 0) {
			/* printf("%s %s\n", key, value); */
			strncpy(product_label, value, LABEL_LEN);
		} else if (strcmp(key, "manufact_lab") == 0) {
			/* printf("%s %s\n", key, value); */
			strncpy(manufact_label, value, LABEL_LEN);
		} else if (strcmp(key, "video_name") == 0) {
			 printf("%s %s\n", key, value);
			strncpy(video_device_name, value, LABEL_LEN);
		} else if (strcmp(key, "audio_name") == 0) {
			 printf("%s %s\n", key, value);
			strncpy(audio_device_name, value, LABEL_LEN);
		} else if (strcmp(key, "rcmode") == 0) {
			/* printf("%s %s\n", key, value); */
#ifdef T31
			if (strcmp(value, "vbr") == 0) {
				g_RcMode = IMP_ENC_RC_MODE_VBR;
			} else if (strcmp(value, "cbr") == 0) {
				g_RcMode = IMP_ENC_RC_MODE_CBR;
			} else if (strcmp(value, "fixqp") == 0) {
				g_RcMode = IMP_ENC_RC_MODE_FIXQP;
			} else if (strcmp(value, "cappedvbr") == 0) {
				g_RcMode = IMP_ENC_RC_MODE_CAPPED_VBR;
			} else {
				printf("Invalid RC method: %s\n", value);
			}
#else
			if (strcmp(value, "vbr") == 0) {
				g_RcMode = ENC_RC_MODE_VBR;
			} else if (strcmp(value, "cbr") == 0) {
				g_RcMode = ENC_RC_MODE_CBR;
			} else if (strcmp(value, "fixqp") == 0) {
				g_RcMode = ENC_RC_MODE_FIXQP;
			} else {
				printf("Invalid RC method: %s\n", value);
			}
#endif
		} else if (strcmp(key, "nframes") == 0) {
			/* printf("%s %s\n", key, value); */
			i = 0;
			struct Ucamera_YUYV_Param *yuyvl;
			struct Ucamera_JPEG_Param *jpegl;
			struct Ucamera_H264_Param *h264l;
			nframes = strToInt(value);
			vcfg->yuyvnum = nframes;
			vcfg->jpegnum = nframes;
			vcfg->h264num = nframes;
			yuyvl = (Ucamera_YUYV_Param*)malloc(vcfg->yuyvnum*sizeof(struct Ucamera_YUYV_Param));
			jpegl = (Ucamera_JPEG_Param*)malloc(vcfg->jpegnum*sizeof(struct Ucamera_JPEG_Param));
			h264l = (Ucamera_H264_Param*)malloc(vcfg->h264num*sizeof(struct Ucamera_H264_Param));
			intervals = 10000000/g_Fps_Num;

			while (i < nframes) {
				if (fscanf(uvc_config_fd, "%[^\n]", line_str) < 0)
					break;
				sscanf(line_str, "{%d, %d}", &width, &height);

				if (width > 0 && height > 0 && (width%16 == 0)) {

					yuyvl[i].width = width;
					yuyvl[i].height = height;
					yuyvl[i].fps_num = intervals;
					jpegl[i].width = width;
					jpegl[i].height = height;
					jpegl[i].fps_num = intervals;
					h264l[i].width = width;
					h264l[i].height = height;
					h264l[i].fps_num = intervals;
				} else {
					printf("error(%s %d)Invalid width or height(%d %d)\n", __func__, __LINE__, width, height);
				}
				i++;
				fseek(uvc_config_fd , 1, SEEK_CUR);
			}
			vcfg->yuyvlist = yuyvl;
			vcfg->jpeglist = jpegl;
			vcfg->h264list = h264l;
		} else {
			printf("Invalid config param: %s\n", key);
		}

	}
	/* printf("******************************\n"); */
	free(line_str);
	return 0;
}

static int kiva_generate_version()
{
	FILE *fp = NULL;
	int ret = -1;
	static char Kiva_version[LABEL_LEN] = "v1.1.1";
	if ((fp = fopen("/tmp/version.txt", "w+")) == NULL) {
		Ucamera_LOG("open config file failed %s!\n", __func__);
		return -1;
	}

	ret = fwrite(Kiva_version, 1, strlen(Kiva_version), fp);
	if(ret != strlen(Kiva_version))
	{
		Ucamera_LOG("write  config file failed %s!\n", __func__);
		fclose(fp);
		return -1;
	}
	fclose(fp);
	return 0;
}

int speak_volume_write_config(int val)
{
	char key[8] = {0};
	char value[24] = "speak_volume";
	char line_str[128] = {0};
	int line_num = 0;

	fseek(uvc_config_fd , 0L, SEEK_SET);
	while (!feof(uvc_config_fd)) {
		if (fscanf(uvc_config_fd, "%[^\n]", line_str) < 0){
			break;
		} else {
			fseek(uvc_config_fd , 1, SEEK_CUR);
			line_num++;
		}

		if (line_num == spk_volume_line - 1) {
			fprintf(uvc_config_fd, "%s    :%03d", value, val);
			} else
				continue;
		}
	return 0;

}

static int uvc_event_process(int event_id, void *data)
{
	unsigned int intervals = 0;

	switch (event_id) {
	case UCAMERA_EVENT_STREAMON: {
		struct Ucamera_Video_Frame *frame = (struct Ucamera_Video_Frame *) data;
		Ucam_Stream_On = 1;
		intervals = frame->intervals;
		g_Fps_Num = 10000000 / intervals;
		if (g_led)
			sample_ucamera_led_ctl(g_led, 0);
#if 0
	int retry_cnt = 0;
imp_init_check:
		if (!imp_inited) {
			if (retry_cnt++ > 40) {
				Ucamera_LOG("[error]imp system init failed.\n");
				return 0;
			}
			Ucamera_LOG("imp sys not ready, wait and retry:%d", retry_cnt);
			usleep(50*1000);
			goto imp_init_check;
		}
#endif

			return imp_sdk_init(frame->fcc, frame->width, frame->height);
			//return 0;
	}
	case UCAMERA_EVENT_STREAMOFF: {
		struct Ucamera_Video_Frame *frame = (struct Ucamera_Video_Frame *) data;
		Ucam_Stream_On = 0;
		if (g_led)
			sample_ucamera_led_ctl(g_led, 1);

		return imp_sdk_deinit(frame->fcc);
	}

	default:
		Ucamera_LOG("%s(ERROR): unknown message ->[%d]!\n", TAG, event_id);
		return -1;
	};

	return 0;
}

FILE *pcm_fd = NULL;
FILE *ref_fd = NULL;
short ref_pcm[640] = {0};
int sample_audio_pcm_get(short *pcm)
{
	int len;
#if 0
	static int i = 0;
	if (pcm_fd == NULL)
		pcm_fd = fopen("/tmp/dmic.pcm", "wb");
	if (ref_fd == NULL)
		ref_fd = fopen("/tmp/ref.pcm", "wb");

#endif
	len = sample_audio_amic_pcm_get(ref_pcm);
#ifdef T31
	len = sample_audio_dmic_pcm_get(pcm);
#endif
#if 0
	if (i++ < 1200) {
		if (pcm_fd)
			fwrite(pcm, 2, len/2, pcm_fd);
		if (ref_fd)
			fwrite(ref_pcm, 2, len/2, ref_fd);
	}
#endif
	return len;
}


#ifndef T20
void signal_handler(int signum) {
	Ucamera_LOG("catch signal %d\n", signum);
	if (signum == SIGUSR1)
		sem_post(&ucam_ready_sem);
	else if(signum == SIGUSR2) {
		Ucamera_LOG("Ucamera Exit Now. \n");
		if(g_Audio == 1){
			sample_audio_amic_exit();
#ifdef T31
			if (g_dmic)
				sample_audio_dmic_exit();
#endif
		}
		imp_system_exit();
		Ucamera_DeInit();
		/*Ucamera_Video_Stop();*/
	}
}

void *ucam_impsdk_init_entry(void *res)
{
	int i;
	//sem_wait(&ucam_ready_sem);

	//sample_system_init();

	imp_inited = 1;
	if (g_Audio == 1) {
		if (g_dmic)
			sample_audio_dmic_init();
		sample_audio_amic_init();
		Ucamera_Audio_Start();
	}

	if (g_Speak)
		sample_audio_play_start();
	
    //imp_sdk_init(V4L2_PIX_FMT_MJPEG,1920,1080);
    return NULL;
}
#endif

#ifndef T20
static void motor_reset(int pos, int fd, int tar_pos)
{
	int ret = -1;
	if(pos % 25 != 0) {
		pos = (pos / 25) * 25;
	}
	while (pos != tar_pos) {
		if (pos > tar_pos) {
			while (pos != tar_pos) {
				pos -= 25;
				ret = ioctl(fd , MOTOR_MOVE , pos);
				if(ret < 0){
					Ucamera_LOG("ERROR(%s, %d): ioctl err!\n", __func__, __LINE__);
					return;
				}
				usleep(10 * 1000);
			}
		} else {
			while (pos != tar_pos) {
				pos += 25;
				ret = ioctl(fd , MOTOR_MOVE , pos);
				if (ret < 0) {
					Ucamera_LOG("ERROR(%s, %d): ioctl err!\n", __func__, __LINE__);
					return;
				}
				usleep(10 * 1000);
			}
		}
	}
	dac_pos = tar_pos;

	return;
}

/*	module function: Autofocus algorithm	*/
static int autoFocus(int fd, IMPISPAFHist af_hist)
{
    return 0;
}

static void *get_video_clarity(void *args)
{
    return 0;
}
#endif

/* ---------------------------------------------------------------------------
 * main
 */
//struct Ucamera_Cfg ucfg;

//int imp_config(void)
//{
//	memset(&ucfg, 0, sizeof(struct Ucamera_Cfg));
//	if (ucamera_load_config(&ucfg)) 
//		Ucamera_LOG("ucamera load config failed!\n");
//	
//    return 0;
//
//}

int uvc_system_init(void)
{
    struct Ucamera_Cfg ucfg;
	struct Ucamera_Video_CB_Func v_func;
	struct Ucamera_Audio_CB_Func a_func;
	memset(&ucfg, 0, sizeof(struct Ucamera_Cfg));
	memset(&v_func, 0, sizeof(struct Ucamera_Video_CB_Func));
	memset(&a_func, 0, sizeof(struct Ucamera_Audio_CB_Func));

#if 0
	if (AntiCopy_Verify()) {
		Ucamera_LOG("AntiCopy Verified failed!!!\n");
		return 0;
	}
#endif
	if (ucamera_load_config(&ucfg)) {
		Ucamera_LOG("ucamera load config failed!\n");
		return 0;
	}

    UVC_START_FLAG=true;

//	if (ucamera_uvc_pu_attr_load()) {
//		Ucamera_LOG("[ERROR] load uvc PU attr failed.\n");
//		return 0;
//	}

	if (kiva_generate_version())
		Ucamera_LOG("[Warning] kiva generate version is failed\n");

	if (g_Speak)
		ucfg.pcfg.Pid = PRODUCT_ID + 1;
	else
		ucfg.pcfg.Pid = PRODUCT_ID;
	ucfg.pcfg.Vid = VENDOR_ID;
	ucfg.pcfg.version = DEVICE_BCD;
	strcpy(ucfg.pcfg.manufacturer, manufact_label);
	strcpy(ucfg.pcfg.product, product_label);
	strcpy(ucfg.pcfg.serial, serial_label);
	strcpy(ucfg.pcfg.video_name, video_device_name);
	strcpy(ucfg.pcfg.audio_name, audio_device_name);

	ucfg.acfg.mic_volume = Dec_to_Hex(Mic_Volume);
	ucfg.acfg.spk_volume = Dec_to_Hex(Spk_Volume);

	Ucamera_Config(&ucfg);
	Ucamera_Init(UVC_BUF_NUM, UVC_BUF_SIZE);

	v_func.get_YuvFrame = sample_get_yuv_snap;
	v_func.get_JpegFrame = sample_get_jpeg_snap;
	v_func.get_H264Frame = sample_get_h264_snap;
	Ucamera_Video_Regesit_CB(&v_func);







	UCamera_Registe_Event_Process_CB(uvc_event_process);
	UCamera_Video_Start();


    int ret = -1;
	pthread_t ucam_impsdk_init_id;
	ret = pthread_create(&ucam_impsdk_init_id, NULL, ucam_impsdk_init_entry, NULL);
	if (ret != 0) {
		Ucamera_LOG("pthread_create failed \n");
		return -1;
	}



	while (1) {
        	 usleep(1000*1000*5);
	}
	return 0;
}
