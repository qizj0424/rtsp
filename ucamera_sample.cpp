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
static struct Ucamera_Zoom_Cfg zoom_cfg;

FILE *uvc_attr_fd = NULL;
FILE *uvc_config_fd = NULL;
pthread_mutex_t uvc_attr_mutex;
sem_t ucam_ready_sem;
#ifndef T20
static int imp_inited = 0;
#endif
IMPISPGamma gamma_cur;

struct uvc_pu_string {
	char id;
	const char *s;
};

struct uvc_pu_string pu_string[] = {
	{0, "null"},
	{UVC_BACKLIGHT_COMPENSATION_CONTROL, "backlight"},
	{UVC_BRIGHTNESS_CONTROL, "brightness"},
	{UVC_CONTRAST_CONTROL, "contrast"},
	{UVC_GAIN_CONTROL, "gain"},
	{UVC_POWER_LINE_FREQUENCY_CONTROL, "powerline_freq"},
	{UVC_HUE_CONTROL, "hue"},
	{UVC_SATURATION_CONTROL, "saturation"},
	{UVC_SHARPNESS_CONTROL, "sharpness"},
	{UVC_GAMMA_CONTROL, "gamma"},
	{UVC_WHITE_BALANCE_TEMPERATURE_CONTROL, "white_balance"},
	{UVC_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL, "white_balance_auto"}
};

static struct Ucamera_Video_CT_Control auto_exposure_mode = {
	.type = UVC_AUTO_EXPOSURE_MODE_CONTROL,
};

static struct Ucamera_Video_CT_Control exposure_time = {
	.type = UVC_EXPOSURE_TIME_CONTROL,
};

static struct Ucamera_Video_CT_Control focus = {
	.type = UVC_FOCUS_CONTROL,
};

static struct Ucamera_Video_CT_Control focus_auto = {
    .type = UVC_FOCUS_AUTO_CONTROL,
};

static struct Ucamera_Video_CT_Control zoom = {
	.type = UVC_ZOOM_ABSOLUTE_CONTROL,
};

static struct Ucamera_Video_PU_Control backlight_compens = {
	.type = UVC_BACKLIGHT_COMPENSATION_CONTROL,
};

static struct Ucamera_Video_CT_Control *Ct_ctrl[] = {
    &exposure_time,
    &auto_exposure_mode,
    &focus,
    &focus_auto,
    &zoom,
    NULL,
};

static struct Ucamera_Video_PU_Control brightness = {
	.type = UVC_BRIGHTNESS_CONTROL,
};

static struct Ucamera_Video_PU_Control contrast = {
	.type = UVC_CONTRAST_CONTROL,
};

static struct Ucamera_Video_PU_Control saturation = {
	.type = UVC_SATURATION_CONTROL,
};

static struct Ucamera_Video_PU_Control sharpness = {
	.type = UVC_SHARPNESS_CONTROL,
};

static struct Ucamera_Video_PU_Control hue = {
	.type = UVC_HUE_CONTROL,
};

static struct Ucamera_Video_PU_Control gamma_pu = {
	.type = UVC_GAMMA_CONTROL,
};

static struct Ucamera_Video_PU_Control whitebalance = {
	.type = UVC_WHITE_BALANCE_TEMPERATURE_CONTROL,
};

static struct Ucamera_Video_PU_Control powerlinefreq = {
	.type = UVC_POWER_LINE_FREQUENCY_CONTROL,
};

static struct Ucamera_Video_PU_Control *Pu_ctrl[] = {
	&backlight_compens,
	&brightness,
	&contrast,
	&hue,
	&saturation,
	&sharpness,
	&gamma_pu,
	&whitebalance,
	&powerlinefreq,
	NULL,
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
int sample_video_focus_set(int value)
{
    focus.data[UVC_CUR] = value;
    return 0;
}

int sample_video_focus_get()
{
	return focus.data[UVC_CUR];
}

int sample_video_focus_auto_set(int value)
{
    focus_auto.data[UVC_CUR] = value;
    return 0;
}

int sample_video_focus_auto_get()
{
	return focus_auto.data[UVC_CUR];
}
#ifdef T31
int sample_video_exposure_time_set(int value)
{
    int ret, value_time=0;
    if(value > 900)
	    value_time = 255;
    else if(value > 600)
	    value_time = 220;
    else if(value > 300)
	    value_time = 190;
    else
	    value_time = value;

    ret = IMP_ISP_Tuning_SetAeComp(value_time);
    if(ret)
        Ucamera_LOG("ERROR:set exposure time Invalid leave:%d\n",value);

    exposure_time.data[UVC_CUR] = value;
    return 0;
}

int sample_video_exposure_time_get(void)
{
	return exposure_time.data[UVC_CUR];
}

int sample_video_auto_exposure_mode_set(int value)
{
    int ret;
    if(value == 2){
	    ret = IMP_ISP_Tuning_SetAeComp(128);
	    if(ret)
		    Ucamera_LOG("ERROR:set exposure time Invalid leave:%d\n",value);
    }
	auto_exposure_mode.data[UVC_CUR] = value;
	return 0;
}

int sample_video_auto_exposure_mode_get(void)
{
	return auto_exposure_mode.data[UVC_CUR];
}

int sample_video_zoom_set(int value)
{
	int ret;
	float value_cur;
	int zoomwidth_cur,zoomheight_cur;
	value_cur = value;

	zoomwidth_cur = round(g_VideoWidth/sqrt(value_cur/100));
	zoomwidth_cur = round((float)(zoomwidth_cur/4)) * 4;
	zoomheight_cur = round(g_VideoHeight/sqrt(value_cur/100));
	zoomheight_cur = round((float)(zoomheight_cur/4)) * 4;

	fcrop_obj.fcrop_enable = 1;
	fcrop_obj.fcrop_left = round((float)(g_VideoWidth - zoomwidth_cur) / 2);
	fcrop_obj.fcrop_top = round((float)(g_VideoHeight -  zoomheight_cur) / 2);
	fcrop_obj.fcrop_width = zoomwidth_cur;
	fcrop_obj.fcrop_height = zoomheight_cur;

	ret = IMP_ISP_Tuning_SetFrontCrop(&fcrop_obj);
	if (ret < 0) {
		Ucamera_LOG("IMP Set Fcrop failed=%d\n",__LINE__);
		return -1;
	}
	zoom.data[UVC_CUR] = value;

	return 0;
}

int sample_video_zoom_get(void)
{
	return zoom.data[UVC_CUR];
}
#endif
#ifdef T21
int sample_video_exposure_time_set(int value)
{
	int ret;
	ret = IMP_ISP_Tuning_SetBrightness(value);
	if(ret)
		Ucamera_LOG("ERROR:set exposure time Invalid leave:%d\n",value);

	exposure_time.data[UVC_CUR] = value;
	return 0;
}

int sample_video_exposure_time_get(void)
{
	int ret,value = 0;
	if(!Ucam_Stream_On)
		return exposure_time.data[UVC_CUR];

	ret = IMP_ISP_Tuning_GetBrightness(&value);
	if(ret)
		Ucamera_LOG("ERROR:get exposure time Invalid leave:%d\n",value);
	return value;
}

int sample_video_auto_exposure_mode_set(int value)
{
	int ret;
	ret = IMP_ISP_Tuning_SetBrightness (128);
	if(ret)
	Ucamera_LOG("ERROR:set exposure time Invalid leave:%d\n",value);
	auto_exposure_mode.data[UVC_CUR] = value;
	return 0;
}

int sample_video_auto_exposure_mode_get(void)
{
	return auto_exposure_mode.data[UVC_CUR];
}
#endif

int sample_video_backlight_compens_set(int value)
{
	int ret;
	if (value < 0 || value > 10) {
		Ucamera_LOG("set BackLight Invalid leave:%d\n", value);
		return -1;
	}
	ret = IMP_ISP_Tuning_SetHiLightDepress(0);
	if (ret)
		Ucamera_LOG("ERROR: set BackLight Invalid leave:%d\n", value);
	backlight_compens.data[UVC_CUR] = value;
	ucamera_uvc_pu_attr_save(UVC_BACKLIGHT_COMPENSATION_CONTROL, value);

	return 0;
}

int sample_video_backlight_compens_get(void)
{
	uint32_t value = 0;

	if (!Ucam_Stream_On)
		return backlight_compens.data[UVC_CUR];
#ifdef T31
	int ret = -1;
	ret = IMP_ISP_Tuning_GetHiLightDepress(&value);
	if (ret)
		Ucamera_LOG("ERROR: get BackLight error:%d\n", ret);
#endif
	return value;
}

int sample_video_brightness_set(int value)
{
	int ret;
	unsigned char bright = 128;

	//bright = value & 0xff;
	ret = IMP_ISP_Tuning_SetBrightness(bright);
	if (ret)
		Ucamera_LOG("ERROR: set BrightNess failed :%d\n", ret);
	brightness.data[UVC_CUR] = bright;
	ucamera_uvc_pu_attr_save(UVC_BRIGHTNESS_CONTROL, bright);
	return 0;
}

int sample_video_brightness_get(void)
{
	int ret;
	unsigned char bright = 0;

	if (!Ucam_Stream_On)
		return brightness.data[UVC_CUR];

	ret = IMP_ISP_Tuning_GetBrightness(&bright);
	if (ret) {
		Ucamera_LOG("get BrightNess failed:%d\n", ret);
		bright = 128;
	}
	return bright;
}

int sample_video_contrast_set(int value)
{
	int ret;
	unsigned char tmp = 128;

//	tmp = value & 0xff;
	ret = IMP_ISP_Tuning_SetContrast(tmp);
	if (ret)
		Ucamera_LOG("set Contrast failed:%d\n", ret);

	contrast.data[UVC_CUR] = tmp;
	ucamera_uvc_pu_attr_save(UVC_CONTRAST_CONTROL, tmp);
	return 0;
}

int sample_video_contrast_get(void)
{
	int ret;
	unsigned char cost = 0;

	if (!Ucam_Stream_On)
	return contrast.data[UVC_CUR];

	ret = IMP_ISP_Tuning_GetContrast(&cost);
	if (ret) {
		Ucamera_LOG("get Contrast failed:%d\n", ret);
		cost = 128;
	}
	return cost;
}

int sample_video_saturation_set(int value)
{
	int ret;
	unsigned char tmp = 128;
	//tmp = value & 0xff;

	ret = IMP_ISP_Tuning_SetSaturation(tmp);
	if (ret)
		Ucamera_LOG("set Saturation failed:%d\n", ret);
	saturation.data[UVC_CUR] = tmp;
	ucamera_uvc_pu_attr_save(UVC_SATURATION_CONTROL, tmp);
	return 0;
}

int sample_video_saturation_get(void)
{
	int ret;
	unsigned char tmp = 0;

	if (!Ucam_Stream_On)
		return saturation.data[UVC_CUR];

	ret = IMP_ISP_Tuning_GetSaturation(&tmp);
	if (ret) {
		Ucamera_LOG("get Saturation failed:%d\n", ret);
		tmp = 128;

	}
	return tmp;
}

int sample_video_sharpness_set(int value)
{
	int ret;
	unsigned char tmp = 128;

//	tmp = value & 0xff;
	ret = IMP_ISP_Tuning_SetSharpness(tmp);
	if (ret)
		Ucamera_LOG("set Sharpness failed:%d\n", ret);
	sharpness.data[UVC_CUR] = tmp;
	ucamera_uvc_pu_attr_save(UVC_SHARPNESS_CONTROL, tmp);
	return 0;
}

int sample_video_sharpness_get(void)
{
	int ret;
	unsigned char tmp = 0;

	if (!Ucam_Stream_On)
		return sharpness.data[UVC_CUR];

	ret = IMP_ISP_Tuning_GetSharpness(&tmp);
	if (ret) {
		Ucamera_LOG("get Sharpness failed:%d\n", ret);
		tmp = 128;
	}
	return tmp;
}
#ifdef T31
int sample_video_hue_set(int value)
{
	int ret;
	unsigned char hue_value = value & 0xff;
	ret = IMP_ISP_Tuning_SetBcshHue(hue_value);
	if (ret)
		Ucamera_LOG("set Hue failed:%d\n", ret);
	hue.data[UVC_CUR] = hue_value;
	ucamera_uvc_pu_attr_save(UVC_HUE_CONTROL, hue_value);
	return 0;
}

int sample_video_hue_get(void)
{
	int ret;
	unsigned char hue_value = 0;

	if (!Ucam_Stream_On)
		return hue.data[UVC_CUR];
	ret = IMP_ISP_Tuning_GetBcshHue(&hue_value);
	if (ret) {
		Ucamera_LOG("set Hue failed:%d\n", ret);
		hue_value = 128;
	}
	return hue_value;
}
#endif
#if 0
int sample_video_gamma_set(int value)
{
	int ret;
	unsigned char tmp = 0;

	tmp = value & 0xff;
	ret = IMP_ISP_Tuning_SetContrast(tmp);
	if (ret)
		Ucamera_LOG("set Contrast failed:%d\n", ret);

	gamma_pu.data[UVC_CUR] = tmp;
	ucamera_uvc_pu_attr_save(UVC_GAMMA_CONTROL, tmp);
	return 0;
}

int sample_video_gamma_get(void)
{
	int ret;
	unsigned char cost = 0;

	if (!Ucam_Stream_On)
	return gamma_pu.data[UVC_CUR];

	ret = IMP_ISP_Tuning_GetContrast(&cost);
	if (ret) {
		Ucamera_LOG("get Contrast failed:%d\n", ret);
		cost = 128;
	}
	return cost;
}
#endif
int sample_video_gamma_set(int value)
{
	int i;
	IMPISPGamma gamma;
	memset(&gamma, 0, sizeof(IMPISPGamma));

	memcpy(&gamma, &gamma_cur, sizeof(IMPISPGamma));
	gamma.gamma[64] = gamma_cur.gamma[64] + 3*(value - 400);
	//IMP_ISP_Tuning_SetGamma(&gamma);

	gamma_pu.data[UVC_CUR] = value;
	ucamera_uvc_pu_attr_save(UVC_GAMMA_CONTROL, value);
	return 0;
}

int sample_video_gamma_get(void)
{
	return gamma_pu.data[UVC_CUR];
}

/*
* value高16位代表模式（0:自动/ 1:手动
* value 底16位为白平衡实际值
*/
int sample_video_whitebalance_set(int value)
{
	int ret;
	unsigned short gain, mode = 0;
	IMPISPWB wb ={
            .mode = ISP_CORE_WB_MODE_AUTO
        };

	ret = IMP_ISP_Tuning_SetWB(&wb);
	if (ret)
		Ucamera_LOG("set WhiteBalance failed:%d\n", ret);

	whitebalance.data[UVC_CUR] = wb.bgain;
	ucamera_uvc_pu_attr_save(UVC_WHITE_BALANCE_TEMPERATURE_CONTROL, wb.bgain);
	return 0;
}

int sample_video_whitebalance_get(void)
{
	return 0;
}

int sample_video_powerlinefreq_set(int value)
{
	int ret, fps_num;
	uint32_t fps_num_bak,fps_den_bak;
	IMPISPAntiflickerAttr attr;

	attr = (IMPISPAntiflickerAttr)value;
	if (attr < IMPISP_ANTIFLICKER_DISABLE || attr >= IMPISP_ANTIFLICKER_BUTT) {
		Ucamera_LOG("[ERROR]Sample set PowerLine Freq Invalid level:%d\n", value);
		return 0;
	}
	ret = IMP_ISP_Tuning_SetAntiFlickerAttr(attr);
	if (ret)
		Ucamera_LOG("set PowerLine Freq failed:%d\n", ret);
	if (attr == IMPISP_ANTIFLICKER_50HZ)
		fps_num = SENSOR_FRAME_RATE_NUM;
	else if(attr == IMPISP_ANTIFLICKER_60HZ)
		fps_num = SENSOR_FRAME_RATE_NUM_30;
	else
		fps_num = g_Fps_Num;
	ret = IMP_ISP_Tuning_GetSensorFPS(&fps_num_bak, &fps_den_bak);
	if (ret < 0) {
		Ucamera_LOG("failed to get sensor fps\n");
		return -1;
	}
	if(fps_num != fps_num_bak/fps_den_bak){
		ret = IMP_ISP_Tuning_SetSensorFPS(fps_num, 1);
		if (ret < 0) {
			Ucamera_LOG("failed to set sensor fps\n");
			return -1;
		}
	}

	powerlinefreq.data[UVC_CUR] = attr;
	ucamera_uvc_pu_attr_save(UVC_POWER_LINE_FREQUENCY_CONTROL, attr);
	return 0;
}

int sample_video_powerlinefreq_get(void)
{
	int ret;
	IMPISPAntiflickerAttr attr;

	if (!Ucam_Stream_On)
		return powerlinefreq.data[UVC_CUR];

	ret = IMP_ISP_Tuning_GetAntiFlickerAttr(&attr);
	if (ret)
		Ucamera_LOG("get PowerLine Freq faild:%d\n", ret);
	return attr;
}


int uvc_pu_attr_setcur(int type, int value)
{
	int ret = 0;
	struct Ucamera_Video_PU_Control *pu_attr = NULL;

	switch (type) {
	case UVC_BACKLIGHT_COMPENSATION_CONTROL:
		pu_attr = &backlight_compens;
		break;
	case UVC_BRIGHTNESS_CONTROL:
		pu_attr = &brightness;
		break;
	case UVC_CONTRAST_CONTROL:
		pu_attr = &contrast;
		break;
	case UVC_GAIN_CONTROL:
		break;
	case UVC_POWER_LINE_FREQUENCY_CONTROL:
		pu_attr = &powerlinefreq;
		break;
	case UVC_HUE_CONTROL:
		pu_attr = &hue;
		break;
	case UVC_SATURATION_CONTROL:
		pu_attr = &saturation;
		break;
	case UVC_SHARPNESS_CONTROL:
		pu_attr = &sharpness;
		break;
	case UVC_GAMMA_CONTROL:
		pu_attr = &gamma_pu;
		break;
	case UVC_WHITE_BALANCE_TEMPERATURE_CONTROL:
		pu_attr = &whitebalance;
		break;
	case UVC_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL:
		break;
	default:
		Ucamera_LOG("Unkown uvc pu type:%d\n", type);
		ret = -1;
		break;
	}
	if (pu_attr)
		pu_attr->data[UVC_CUR] = value;

	return ret;
}


const char *uvc_pu_type_to_string(int type)
{
	return pu_string[type].s;
}

int uvc_pu_string_to_type(char *string)
{
	int i, index;
	index = sizeof(pu_string)/sizeof(struct uvc_pu_string);

	for (i = 0; i < index; i++) {
		if (strcmp(string, pu_string[i].s) == 0)
			return pu_string[i].id;
	}
	return 0;
}

int ucamera_uvc_pu_attr_load(void)
{
	char key[64] = {0};
	char value[16] = {0};
	char line_str[128] = {0};
	int type;


	if ((uvc_attr_fd = fopen("/system/config/uvc.attr", "r+")) == NULL) {
		Ucamera_LOG("%s open config file failed!\n", __func__);
		return -1;
	}
	if (pthread_mutex_init(&uvc_attr_mutex, NULL) != 0){
		Ucamera_LOG("%s init mutex failed\n", __func__);
		return -1;
	}
	while (!feof(uvc_attr_fd)) {
		if (fscanf(uvc_attr_fd, "%[^\n]", line_str) < 0)
			break;

		if (sscanf(line_str, "%[^:]:%[^\n]", key, value) != 2) {
			Ucamera_LOG("warning: skip config %s\n", line_str);
			fseek(uvc_attr_fd , 1L, SEEK_CUR);
			continue;
		}
		/* Ucamera_LOG("%s : %d\n", key, strToInt(value)); */
		if ((type = uvc_pu_string_to_type(key))) {
			uvc_pu_attr_setcur(type, strToInt(value));
		}
		fseek(uvc_attr_fd , 1L, SEEK_CUR);
	}

	return 0;
}

int ucamera_uvc_pu_attr_save(int type, int value)
{
	char key[64] = {0};
	char data[16] = {0};
	char line_str[128] = {0};
	const char *attr_string = NULL;


	if (uvc_attr_fd == NULL) {
		Ucamera_LOG("%s can not open uvc config file!\n", __func__);
		return -1;
	}
	attr_string = uvc_pu_type_to_string(type);
	if (attr_string == NULL)
		return -1;

	pthread_mutex_lock(&uvc_attr_mutex);
	fseek(uvc_attr_fd, 0L, SEEK_SET);

	while (!feof(uvc_attr_fd)) {
		if (fscanf(uvc_attr_fd, "%[^\n]", line_str) < 0)
			break;

		if (sscanf(line_str, "%[^:]:%[^\n]", key, data) != 2) {
			Ucamera_LOG("warning: Invalid param:%s\n", line_str);
			fseek(uvc_attr_fd , 1L, SEEK_CUR);
			continue;
		}
		if (strcmp(key, attr_string) == 0) {
			fseek(uvc_attr_fd, -strlen(line_str), SEEK_CUR);
			fprintf(uvc_attr_fd, "%s:%04d", key, value);
		}
		fseek(uvc_attr_fd , 1L, SEEK_CUR);
	}

	pthread_mutex_unlock(&uvc_attr_mutex);
	return 0;
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

int imp_isp_tuning_init(void)
{
	int ret, fps_num;

	/* enable turning, to debug graphics */
	ret = IMP_ISP_EnableTuning();
	if(ret < 0){
		IMP_LOG_ERR(TAG, "IMP_ISP_EnableTuning failed\n");
		return -1;
	}

	IMP_ISP_Tuning_SetContrast(128);
	IMP_ISP_Tuning_SetSharpness(128);
	IMP_ISP_Tuning_SetSaturation(128);
	IMP_ISP_Tuning_SetBrightness(128);
	IMP_ISP_Tuning_SetHiLightDepress(0);
#ifdef T31
	/*IMP_ISP_Tuning_SetFrontCrop(&fcrop_obj);*/
    	IMP_ISP_Tuning_SetBcshHue(hue.data[UVC_CUR]);

	if(auto_exposure_mode.data[UVC_CUR] == 4)
		sample_video_exposure_time_set(exposure_time.data[UVC_CUR]);
	else
		IMP_ISP_Tuning_SetAeComp(128);
#endif

	IMPISPAntiflickerAttr attr = IMPISP_ANTIFLICKER_DISABLE;
	if (attr < IMPISP_ANTIFLICKER_DISABLE || attr >= IMPISP_ANTIFLICKER_BUTT) {
		Ucamera_LOG("%s unvaild Antiflicker param:%d\n", __func__, attr);
		attr = IMPISP_ANTIFLICKER_DISABLE;
	}
	IMP_ISP_Tuning_SetAntiFlickerAttr(attr);

	if (attr == IMPISP_ANTIFLICKER_50HZ)
		fps_num = SENSOR_FRAME_RATE_NUM;
	else if(attr == IMPISP_ANTIFLICKER_60HZ)
		fps_num = SENSOR_FRAME_RATE_NUM_30;
	else
		fps_num = g_Fps_Num;

	ret = IMP_ISP_Tuning_SetSensorFPS(fps_num, 1);
	if (ret < 0) {
		Ucamera_LOG("failed to set sensor fps\n");
		return -1;
	}
	Ucamera_LOG("set Antiflicker level:%d fps:%d\n", attr, fps_num);

	/* update ev status(used by dynamic_fps) */
	ev_last = 0;

	ret = IMP_ISP_Tuning_SetISPRunningMode(IMPISP_RUNNING_MODE_DAY);
	if (ret < 0){
		IMP_LOG_ERR(TAG, "failed to set running mode\n");
		return -1;
	}

#ifdef T31
	if ((g_HV_Flip > IMPISP_FLIP_NORMAL_MODE) && (g_HV_Flip < IMPISP_FLIP_MODE_BUTT)) {
		IMPISPHVFLIP testhv = IMPISP_FLIP_NORMAL_MODE;
		IMP_ISP_Tuning_SetHVFLIP(testhv);
		if (ret < 0){
			IMP_LOG_ERR(TAG, "failed to set HV Filp mode\n");
			return -1;
		}
		IMP_LOG_ERR(TAG, "Set HV filp mode:%d\n", g_HV_Flip);

		usleep(100*1000);
	}
#else
	if (g_HV_Flip) {
		if (g_HV_Flip == 1)
			IMP_ISP_Tuning_SetISPHflip(IMPISP_TUNING_OPS_MODE_ENABLE);
		else if (g_HV_Flip == 2)
			IMP_ISP_Tuning_SetISPVflip(IMPISP_TUNING_OPS_MODE_ENABLE);
		else if (g_HV_Flip == 3){
			IMP_ISP_Tuning_SetISPHflip(IMPISP_TUNING_OPS_MODE_ENABLE);
			IMP_ISP_Tuning_SetISPVflip(IMPISP_TUNING_OPS_MODE_ENABLE);
		}else
			IMP_LOG_ERR(TAG, "Anvailed HV Filp mode :%d\n", g_HV_Flip);
	}
#endif

	return 0;

}

int imp_sdk_init(int format, int width, int height)
{
	int i, ret;
	IMPFSChnAttr *imp_chn_attr_tmp;
	Ucamera_LOG("IMP SDK reinit!\n");
	zoom_cfg.width = width;
	zoom_cfg.height = height;

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

	if (g_Power_save) {
		ret = IMP_ISP_EnableSensor();
		if(ret < 0){
			IMP_LOG_ERR(TAG, "failed to EnableSensor\n");
			return -1;
		}
		imp_isp_tuning_init();
	}


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
		IMP_ISP_Tuning_GetGamma(&gamma_cur);
		sample_video_gamma_set(gamma_pu.data[UVC_CUR]);
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
    printf("---------->format = %d<------------\n");
	switch (format) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_NV12:
		break;
	case V4L2_PIX_FMT_MJPEG:
		ret = sample_jpeg_init();
	//	break;
	//case V4L2_PIX_FMT_H264:
		ret = encoder_init_demo();
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
	IMP_ISP_Tuning_GetGamma(&gamma_cur);
	sample_video_gamma_set(gamma_pu.data[UVC_CUR]);

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
	focus.data[UVC_CUR] = 100;
	zoom.data[UVC_CUR] = 100;

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
			focus.data[UVC_MAX] = strToInt(value);
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
               zjqi("--------------->21<-------------");

	switch (event_id) {
	case UCAMERA_EVENT_STREAMON: {
		struct Ucamera_Video_Frame *frame = (struct Ucamera_Video_Frame *) data;
		Ucam_Stream_On = 1;
		intervals = frame->intervals;
		g_Fps_Num = 10000000 / intervals;
		if (g_led)
			sample_ucamera_led_ctl(g_led, 0);
#ifndef T20
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

			//return imp_sdk_init(frame->fcc, frame->width, frame->height);
			return 0;
	}
	case UCAMERA_EVENT_STREAMOFF: {
		struct Ucamera_Video_Frame *frame = (struct Ucamera_Video_Frame *) data;
		Ucam_Stream_On = 0;
		if (g_led)
			sample_ucamera_led_ctl(g_led, 1);
		//IMP_ISP_Tuning_SetGamma(&gamma_cur);

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
               zjqi("--------------->12<-------------");

	sample_system_init();

	imp_inited = 1;
	if (g_Audio == 1) {
#ifdef T31
		if (g_dmic)
			sample_audio_dmic_init();
#endif
		sample_audio_amic_init();
		Ucamera_Audio_Start();
	}

	if (g_Speak)
		sample_audio_play_start();
	
    imp_sdk_init(V4L2_PIX_FMT_MJPEG,1920,1080);
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
	int i;
	int ret = -1;
	int dac_clarity = 0;
	int max_value = 0;
	int min_value = 0;
	int max_i = 0;
	int refocus_rate = 0;
	int motor_pos_max = focus.data[UVC_MAX];
	int dac_clarity_buf[32] = {0};

	/* step1: motor reset motor_pos_max */
	motor_reset(dac_pos, fd, motor_pos_max);
	usleep(motor_sleep_time);

	ret = IMP_ISP_Tuning_GetAfHist(&af_hist);
	if (ret < 0) {
		Ucamera_LOG("IMP_ISP_Tuning_GetAFMetrices err! \n");
		return -1;
	}
	dac_clarity_buf[motor_pos_max / motor_step] = af_hist.af_stat.af_metrics_alt;
	Ucamera_DEBUG("dac_clarity_buf[%d] is %d \n" , (motor_pos_max / motor_step) ,dac_clarity_buf[motor_pos_max / motor_step]);

	/* step2: motor move from 400 to 100 (step : 50), get max clarity */
	for (i = (motor_pos_max / motor_step - 1); i >= (100 / motor_step); i--) {
		dac_value = motor_step * i;
		motor_reset(dac_pos, fd, dac_value);
		usleep(motor_sleep_time);

		ret = IMP_ISP_Tuning_GetAfHist(&af_hist);
		if (ret < 0) {
			Ucamera_LOG("IMP_ISP_Tuning_GetAFMetrices err! \n");
			return -1;
		}
		dac_clarity_buf[i] = af_hist.af_stat.af_metrics_alt;
		Ucamera_DEBUG("dac_clarity_buf[%d] is %d \n", i, dac_clarity_buf[i]);
	}

	max_value = dac_clarity_buf[100 / motor_step];
	max_i = 100 / motor_step;
	for (i = (max_i + 1); i <= (motor_pos_max / motor_step); i++) {
		if (dac_clarity_buf[i] > max_value) {
			max_value = dac_clarity_buf[i];
			max_i = i;
		}
	}

	/* step3: judge focus result by max and min value */
	min_value = dac_clarity_buf[100 / motor_step];
	for (i = (100 / motor_step + 1); i <= (motor_pos_max / motor_step); i++) {
		if (dac_clarity_buf[i] < min_value) {
			min_value = dac_clarity_buf[i];
		}
	}

	if (max_value == 0)
		max_value = 1;

	refocus_rate = 100 * (max_value - min_value) / max_value;
	Ucamera_DEBUG("refocus_rate is %d\n", refocus_rate);

	if (refocus_rate < 10) {
		flag_refocus = 1;
	}

	/* step4 : move motor to suitable position */
	dac_value = max_i * motor_step;
	motor_reset(dac_pos, fd, dac_value);
	Ucamera_DEBUG("motor_postion is %d\n", dac_value);

	/* step5: refocus judgement if the moter is at near focal position */
	if (dac_value >= 200) {
		sem_post(&ucam_Af_sem);
	}

	return max_value;
}

static void *af_check(void *arg)
{
	int fd = *(int *)arg;
	int ret = 0;
	IMPISPAFHist af_hist;
	int dac_clarity;
	while(1) {
		sem_wait(&ucam_Af_sem);
		/*Set the flag to 0 before checking, and determine whether the flag has been modified after a delay of one second*/
		flag_af_check = 0;
		usleep(MOTOR_MOVE_DELAY_TIME);
		pthread_mutex_lock(&uvc_Af_mutex);
		if (flag_af_check == 0) {
			ret = IMP_ISP_Tuning_GetAfHist(&af_hist);
			if (ret < 0) {
				pthread_mutex_unlock(&uvc_Af_mutex);
				Ucamera_LOG("IMP_ISP_Tuning_GetAFMetrices err! \n");
			}
			dac_clarity = af_hist.af_stat.af_metrics_alt;
			motor_reset(dac_pos, fd, 100);
			usleep(motor_sleep_time);
			ret = IMP_ISP_Tuning_GetAfHist(&af_hist);
			if( ret < 0 ){
				pthread_mutex_unlock(&uvc_Af_mutex);
				Ucamera_LOG("IMP_ISP_Tuning_GetAFMetrices err! \n");
			}
			if(dac_clarity > af_hist.af_stat.af_metrics_alt){
				motor_reset(dac_pos, fd, dac_value);
			}
			pthread_mutex_unlock(&uvc_Af_mutex);
		} else {
			pthread_mutex_unlock(&uvc_Af_mutex);
		}

	}
}

static void *get_video_clarity(void *args)
{
	int clarity_cur;
	int clarity_now;
	int clarity_diff;
	int clarity_autoFocus;
	int ret;
	int i,j;
	int fall_rate;
	int num_refocus = 0;
	int flag_count = 0;
	int trigger_count = 0;
	int focus_trigger_value_now = focus_trigger_value;
	int flag_motor_standby = 0;
	IMPISPAFHist af_hist;
	IMPISPWeight af_weight;
	struct timeval ts_s, ts_d;

	sem_init(&ucam_Af_sem, 0, 0);

	/* step1 : initialize clarity algorithm */
	while (Ucam_Stream_On == 0) {
		usleep(1000 * 1000);
	}

	int fd = open("/dev/dw9714", 0);
	if (fd < 0) {
		Ucamera_LOG("ERROR(%s, %d):open err! \n", __func__, __LINE__);
		return NULL;
	}

	while(IMP_ISP_Tuning_GetAfHist(&af_hist) < 0){
		usleep(1000 * 1000);
	}
	Ucamera_LOG("af_enable is %u \n" , af_hist.af_enable);
	Ucamera_LOG("af_metrics_shift is %u \n" , af_hist.af_metrics_shift);
	Ucamera_LOG("af_delta is %u \n" , af_hist.af_delta);
	Ucamera_LOG("af_theta is %u \n" , af_hist.af_theta);
	Ucamera_LOG("af_hilight_th is %u \n" , af_hist.af_hilight_th);
	Ucamera_LOG("af_alpha_alt is %u \n" , af_hist.af_alpha_alt);
	Ucamera_LOG("af_hstart is %u \n" , af_hist.af_hstart);
	Ucamera_LOG("af_vstart is %u \n" , af_hist.af_vstart);
	Ucamera_LOG("af_stat_nodeh is %u \n" , af_hist.af_stat_nodeh);
	Ucamera_LOG("af_stat_nodev is %u \n\n\n" , af_hist.af_stat_nodev);
	unsigned char set_weight_200[15][15] = {
		{0,0,0,0,0,0,0,0,1,1,1,1,1,1,1},
		{0,1,1,1,1,1,1,0,1,1,1,1,1,1,1},
		{0,1,1,1,1,1,1,0,1,1,1,1,1,1,1},
		{0,1,1,8,8,1,1,0,1,1,1,1,1,1,1},
		{0,1,1,8,8,1,1,0,1,1,1,1,1,1,1},
		{0,1,1,1,1,1,1,0,1,1,1,1,1,1,1},
		{0,1,1,1,1,1,1,0,1,1,1,1,1,1,1},
		{0,0,0,0,0,0,0,0,1,1,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},

	};
	unsigned char set_weight_400[15][15] = {
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		{0,0,0,0,1,1,1,1,1,1,1,0,0,0,0},
		{0,0,0,0,1,3,3,3,3,3,1,0,0,0,0},
		{0,0,0,0,1,3,8,8,8,3,1,0,0,0,0},
		{0,0,0,0,1,3,8,8,8,3,1,0,0,0,0},
		{0,0,0,0,1,3,8,8,8,3,1,0,0,0,0},
		{0,0,0,0,1,3,3,3,3,3,1,0,0,0,0},
		{0,0,0,0,1,1,1,1,1,1,1,0,0,0,0},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	};

	if (motor_pixel == 200) {
		for(i = 0; i < 15; i++) {
			for(j = 0; j < 15; j++) {
				af_weight.weight[i][j] = set_weight_200[i][j];
			}
		}
	} else if (motor_pixel == 400) {
		for (i = 0; i < 15; i++) {
			for (j = 0; j < 15; j++) {
				af_weight.weight[i][j] = set_weight_400[i][j];
			}
		}

	} else {
		Ucamera_LOG("set motor_pixel err! \n");
		return NULL;
	}

	ret = IMP_ISP_Tuning_SetAfWeight(&af_weight);
	if (ret < 0) {
		Ucamera_LOG("IMP_ISP_Tuning_SetAfWeight err! \n");
		return NULL;
	}

	ret = IMP_ISP_Tuning_GetAfWeight(&af_weight);
	if (ret < 0) {
		Ucamera_LOG("IMP_ISP_Tuning_GetAfHist err! \n");
		return NULL;
	}
	Ucamera_LOG("af_weight is : \n");

	for (i = 0; i < 15; i++) {
		for (j = 0; j < 15; j++) {
			printf("%u " , af_weight.weight[i][j]);
		}
		printf("\n");
	}

	/*Creat a check thread */
	pthread_t tid_AF_check;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	ret = pthread_create(&tid_AF_check, &attr, af_check, &fd);
	if (ret != 0) {
		Ucamera_LOG("pthread_create err \n");
	}

	clarity_cur = autoFocus(fd , af_hist);
	if (clarity_cur < 0) {
		Ucamera_LOG("autoFocus err! \n");
	}

	while (1) {
		while (Ucam_Stream_On == 0) {
			if(flag_motor_standby == 1) {
				flag_motor_standby = 0;
				ret = ioctl(fd, MOTOR_MOVE, 0);
				if (ret < 0) {
					Ucamera_LOG("ERROR(%s, %d): ioctl err! \n", __func__, __LINE__);
				}
			}
			usleep(1000 * 1000);
		}

		flag_motor_standby = 1;
		if (focus_auto.data[UVC_CUR] == 1) {
			/* autofocus trigger judgement */
			ret = IMP_ISP_Tuning_GetAfHist(&af_hist);
			if (ret < 0) {
				Ucamera_LOG("IMP_ISP_Tuning_GetAFMetrices err! \n");
				continue;
			}
			clarity_now = af_hist.af_stat.af_metrics_alt;
			clarity_diff = abs(clarity_cur - clarity_now);
			fall_rate = (100 * clarity_diff) / clarity_cur;

			if (fall_rate > focus_trigger_value_now) {
				usleep(MOTOR_MOVE_DELAY_TIME);
				trigger_count = 0;
				for (i = 0; i < 5; i++) {
					ret = IMP_ISP_Tuning_GetAfHist(&af_hist);
					if (ret < 0) {
						Ucamera_LOG("IMP_ISP_Tuning_GetAFMetrices err! \n");
						continue;
					}
					clarity_now = af_hist.af_stat.af_metrics_alt;
					clarity_diff = abs(clarity_cur - clarity_now);
					fall_rate = (100 * clarity_diff) / clarity_cur;

					if (fall_rate > focus_trigger_value_now) {
						trigger_count++;
					}
					usleep(100 * 1000);
				}

				Ucamera_DEBUG("trigger_count is %d\n",trigger_count);
				if (trigger_count >= 3) {
					Ucamera_DEBUG("clarity_cur = %d, clarity_now = %d, fall_rate = %d, focus_trigger_value_now = %d\n", clarity_cur, clarity_now, fall_rate, focus_trigger_value_now);
					dac_pos_old = dac_pos;
					gettimeofday(&ts_s , NULL);
					pthread_mutex_lock(&uvc_Af_mutex);
					/*Set the flag to 1 before each entry into autoFocus*/
					flag_af_check = 1;
					clarity_autoFocus = autoFocus(fd , af_hist);
					pthread_mutex_unlock(&uvc_Af_mutex);
					if (clarity_autoFocus < 0) {
						Ucamera_LOG("autoFocus err! \n");
						continue;
					}
					gettimeofday(&ts_d, NULL);
					Ucamera_DEBUG("####AF time = %llums ####\n", ((((unsigned long long )ts_d.tv_sec * 1000000) + ts_d.tv_usec) - ((( unsigned long long)ts_s.tv_sec * 1000000) + ts_s.tv_usec)) / 1000);
					while (flag_refocus == 1) {
						flag_refocus = 0;
						pthread_mutex_lock(&uvc_Af_mutex);
						flag_af_check = 1;
						clarity_autoFocus = autoFocus(fd, af_hist);
						pthread_mutex_unlock(&uvc_Af_mutex);
						num_refocus++;
						if (num_refocus >= 1) {
							flag_refocus = 0;
						}
					}
					num_refocus = 0;
					usleep(100 * 1000);
					ret = IMP_ISP_Tuning_GetAfHist(&af_hist);
					if(ret < 0){
						Ucamera_LOG("IMP_ISP_Tuning_GetAFMetrices err! \n");
						continue;
					}
					clarity_cur = af_hist.af_stat.af_metrics_alt + 1;
					focus_trigger_value_now = (focus_trigger_value - focus_trigger_value * abs(dac_pos - dac_pos_old) / 800);
					Ucamera_DEBUG("focus_trigger_value_now is %d dac_pos is %d dac_pos_old is %d\n",focus_trigger_value_now, dac_pos, dac_pos_old);
				}
			} else {
				usleep(100 * 1000);
			}
		} else {
			usleep(300 * 1000);
			flag_count++;
			if (flag_count >= 10) {
				flag_count = 0;
				IMP_ISP_Tuning_GetAfHist(&af_hist);
				Ucamera_DEBUG("clarity value is %d\n", af_hist.af_stat.af_metrics_alt);
			}
			if (dac_pos != focus.data[UVC_CUR]) {
				dac_pos = focus.data[UVC_CUR];
				ret = ioctl(fd, MOTOR_MOVE, dac_pos);
				if (ret < 0) {
					Ucamera_LOG("ERROR(%s, %d): ioctl err! \n", __func__, __LINE__);
				}
			}
		}
	}
}
#endif

/* ---------------------------------------------------------------------------
 * main
 */

int uvc_system_init(void)
{
	struct Ucamera_Cfg ucfg;
	struct Ucamera_Video_CB_Func v_func;
	struct Ucamera_Audio_CB_Func a_func;
	memset(&ucfg, 0, sizeof(struct Ucamera_Cfg));
	memset(&v_func, 0, sizeof(struct Ucamera_Video_CB_Func));
	memset(&a_func, 0, sizeof(struct Ucamera_Audio_CB_Func));

               zjqi("--------------->1<-------------");
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

	if (ucamera_uvc_pu_attr_load()) {
		Ucamera_LOG("[ERROR] load uvc PU attr failed.\n");
		return 0;
	}

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
	if (g_Speak)
		ucfg.acfg.speak_enable = 1;
	else
		ucfg.acfg.speak_enable = 0;

	Ucamera_Config(&ucfg);
	Ucamera_Init(UVC_BUF_NUM, UVC_BUF_SIZE);

	v_func.get_YuvFrame = sample_get_yuv_snap;
	v_func.get_JpegFrame = sample_get_jpeg_snap;
	v_func.get_H264Frame = sample_get_h264_snap;
	Ucamera_Video_Regesit_CB(&v_func);

	backlight_compens.set = sample_video_backlight_compens_set;
	backlight_compens.get = sample_video_backlight_compens_get;
	brightness.set = sample_video_brightness_set;
	brightness.get = sample_video_brightness_get;
	contrast.set = sample_video_contrast_set;
	contrast.get = sample_video_contrast_get;
	saturation.set = sample_video_saturation_set;
	saturation.get = sample_video_saturation_get;
	sharpness.set = sample_video_sharpness_set;
	sharpness.get = sample_video_sharpness_get;
#ifdef T31
	hue.set = sample_video_hue_set;
	hue.get = sample_video_hue_get;
#endif
	whitebalance.set = sample_video_whitebalance_set;
	whitebalance.get = sample_video_whitebalance_get;
	powerlinefreq.set = sample_video_powerlinefreq_set;
	powerlinefreq.get = sample_video_powerlinefreq_get;
	gamma_pu.set = sample_video_gamma_set;
	gamma_pu.get = sample_video_gamma_get;
	focus.set = sample_video_focus_set;
	focus.get = sample_video_focus_get;
	focus_auto.set = sample_video_focus_auto_set;
	focus_auto.get = sample_video_focus_auto_get;
#ifndef T20
	exposure_time.set = sample_video_exposure_time_set;
	exposure_time.get = sample_video_exposure_time_get;
	auto_exposure_mode.set = sample_video_auto_exposure_mode_set;
	auto_exposure_mode.get = sample_video_auto_exposure_mode_get;
#endif
#ifdef T31
	zoom.set = sample_video_zoom_set;
	zoom.get = sample_video_zoom_get;
#endif

	Ucamera_Video_Regesit_Process_Unit_CB(Pu_ctrl);
	Ucamera_Video_Regesit_Camera_Terminal_CB(Ct_ctrl);


	UCamera_Registe_Event_Process_CB(uvc_event_process);
#ifdef T20
	sample_system_init();
#endif
	UCamera_Video_Start();

	if (g_Audio == 1) {
#ifdef T31
		if (g_dmic)
			a_func.get_AudioPcm = sample_audio_dmic_pcm_get;
		else
			a_func.get_AudioPcm = sample_audio_amic_pcm_get;
#else
			a_func.get_AudioPcm = sample_audio_amic_pcm_get;
#endif
		a_func.set_Mic_Volume = sample_set_mic_volume;
		a_func.set_Spk_Volume = sample_set_spk_volume;
		a_func.set_Mic_Mute = sample_set_mic_mute;
		a_func.set_Spk_Mute = sample_set_spk_mute;
		a_func.get_record_on = sample_get_record_on;
		a_func.get_record_off = sample_get_record_off;
		a_func.get_speak_on = sample_get_speak_on;
		a_func.get_speak_off = sample_get_speak_off;
		Ucamera_Audio_Regesit_CB(&a_func);
#ifdef T20
		Ucamera_Audio_Start();
#endif
	}

	if (g_led)
		sample_ucamera_led_init(g_led);
#ifdef T20
	if (g_Speak)
		sample_audio_play_start();
#endif

#ifndef T20
	int ret;
	sem_init(&ucam_ready_sem, 0, 0);
	signal(SIGUSR1, signal_handler); /*set signal handler*/
	signal(SIGUSR2, signal_handler); /*set signal handler*/
	signal(SIGTERM, signal_handler); /*set signal handler*/
	signal(SIGKILL, signal_handler); /*set signal handler*/

	pthread_t ucam_impsdk_init_id;
	ret = pthread_create(&ucam_impsdk_init_id, NULL, ucam_impsdk_init_entry, NULL);
	if (ret != 0) {
		Ucamera_LOG("pthread_create failed \n");
		return -1;
	}
	pthread_t tid_AF;
	pthread_attr_t attr;
	if(af_on_off == 1){
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		ret = pthread_create(&tid_AF, &attr, get_video_clarity, NULL);
		if (ret != 0) {
			Ucamera_LOG("pthread_create err \n");
			return -1;
		}
	}
#endif



	while (1) {
      	//if (g_Dynamic_Fps)
      		//imp_SensorFPS_Adapture();
        	 usleep(1000*1000*5);
               zjqi("hello uvc!!!");
	}
	return 0;
}
