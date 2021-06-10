/*
*	uvc_gadget.h  --  USB Video Class Gadget driver
 *
 *	Copyright (C) 2009-2010
 *	    Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 */

#ifndef _USBCAMERA_H_
#define _USBCAMERA_H_


#include <stdint.h>

#define UCAMERA_EVENT_STREAMON					1
#define UCAMERA_EVENT_STREAMOFF					2


/*uvc process unit cs*/
#define UVC_BACKLIGHT_COMPENSATION_CONTROL			0x01
#define UVC_BRIGHTNESS_CONTROL					0x02
#define UVC_CONTRAST_CONTROL					0x03
#define UVC_GAIN_CONTROL					0x04
#define UVC_POWER_LINE_FREQUENCY_CONTROL			0x05
#define UVC_HUE_CONTROL						0x06
#define UVC_SATURATION_CONTROL					0x07
#define UVC_SHARPNESS_CONTROL					0x08
#define UVC_GAMMA_CONTROL					0x09
#define UVC_WHITE_BALANCE_TEMPERATURE_CONTROL			0x0a
#define UVC_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL		0x0b
#define UVC_DIGITAL_MULTIPLIER_CONTROL          		0x0e

/*uvc extension unit cs*/
#define UVC_EU_CMD_USR1		0x01
#define UVC_EU_CMD_USR2		0x02
#define UVC_EU_CMD_USR3		0x03
#define UVC_EU_CMD_USR4		0x04
#define UVC_EU_CMD_USR5		0x05
#define UVC_EU_CMD_USR6		0x06
#define UVC_EU_CMD_USR7		0x07
#define UVC_EU_CMD_USR8		0x08


#define UVC_AUTO_EXPOSURE_MODE_CONTROL  0x02
#define UVC_EXPOSURE_TIME_CONTROL       0x04
#define UVC_FOCUS_CONTROL               0x06
#define UVC_FOCUS_AUTO_CONTROL          0x08
#define UVC_ZOOM_ABSOLUTE_CONTROL	0x0b
#define UVC_ROLL_ABSOLUTE_CONTROL	0x0f
#define UVC_PRIVACY_CONTROL		0x11

#define Ucamera_ERR(format, arg...)                                             \
	printf("%s:%s: " format "\n" , "[Ucamera]",  "ERROR:" , ## arg)

#define Ucamera_WARNING(format, arg...)                                         \
	printf("%s:%s: " format "\n" , "[Ucamera]",  "WARNNING:" , ## arg)

#define Ucamera_INFO(format, arg...)                                            \
	printf("%s:%s: " format "\n" , "[Ucamera]",  "INFO:" , ## arg)

#define Ucamera_LOG(format, arg...)                                            \
	printf("%s: " format "\n" , "[Ucamera]", ## arg)

struct Ucamera_YUYV_Param {
	uint32_t width;
	uint32_t height;
	uint32_t fps_num;
};

struct Ucamera_JPEG_Param {
	uint32_t width;
	uint32_t height;
	uint32_t fps_num;
};

struct Ucamera_H264_Param {
	uint32_t width;
	uint32_t height;
	uint32_t fps_num;
};

struct Ucamera_Video_Cfg {
	uint32_t yuyvnum;
	struct Ucamera_YUYV_Param *yuyvlist;
	uint32_t jpegnum;
	struct Ucamera_JPEG_Param *jpeglist;
	uint32_t h264num;
	struct Ucamera_H264_Param *h264list;
};

struct Ucamera_Audio_Cfg {
	int mic_sample_rate;
	int spk_sample_rate;
	short mic_volume;
	short spk_volume;
	int speak_enable;
};

struct Ucamera_Product_Info_Cfg {
	int Pid;
	int Vid;
	int version;
	char manufacturer[64];
	char product[64];
	char serial[64];
	char video_name[64];
	char audio_name[64];
};

struct Ucamera_Cfg {
	struct Ucamera_Video_Cfg vcfg;
	struct Ucamera_Audio_Cfg acfg;
	struct Ucamera_Product_Info_Cfg pcfg;
	int audio_en;
	int adb_en;
	int h264_en;
	int stillcap;

};

struct Ucamera_Video_Frame {
	unsigned int fcc;
	unsigned int width;
	unsigned int height;
	unsigned int intervals;
};

struct Ucamera_Zoom_Cfg {
	uint32_t framenum;
	uint32_t width;
	uint32_t height;
};

struct Ucamera_Audio_Frame {
	unsigned int bitwidth;				/**< 音频采样精度 */
	unsigned int soundmode;				/**< 音频声道模式 */
	char *data;
	int len;							/**< 音频帧长度 */
};

struct Ucamera_Video_CB_Func {
	int (*get_YuvFrame)(char *frame);
	int (*get_JpegFrame)(char *frame);
	int (*get_H264Frame)(char *frame);
};

struct Ucamera_Audio_CB_Func {
	int (*get_AudioPcm)(short *pcm);
	int (*set_Spk_Volume)(int vol);
	int (*set_Mic_Volume)(int vol);
	int (*set_Spk_Mute)(int mute);
	int (*set_Mic_Mute)(int mute);
	int (*get_record_off)();
	int (*get_record_on)();
	int (*get_speak_off)();
	int (*get_speak_on)();

};

#define UVC_CUR					0
#define UVC_MIN					1
#define UVC_MAX					2
#define UVC_RES					3
#define UVC_LEN					4
#define UVC_INFO				5
#define UVC_DEF					6

struct Ucamera_Video_PU_Control {
	int type;
	int cmd;
	int data[8];
	int (*set)(int value);
	int (*get)(void);
};

struct Ucamera_Video_EU_Control {
	int type;
	int len;
	int (*set)(char *data, int len);
	int (*get)(char *data);
};

struct Ucamera_Video_CT_Control {
	int type;
	int cmd;
	int data[8];
	int (*set)(int value);
	int (*get)(void);
};

int Ucamera_Config(struct Ucamera_Cfg *ucfg);

int Ucamera_Init(int FrmNum, int FrmMaxSize);

int Ucamera_Video_Regesit_CB(struct Ucamera_Video_CB_Func *v_func);

int Ucamera_Video_Regesit_Process_Unit_CB(struct Ucamera_Video_PU_Control *puctl[]);
int Ucamera_Video_Regesit_Extension_Unit_CB(struct Ucamera_Video_EU_Control *euctl[]);

int Ucamera_Video_Regesit_Camera_Terminal_CB(struct Ucamera_Video_CT_Control *ctctl[]);

int UCamera_Registe_Event_Process_CB(int (*event_process)(int , void *));

int UCamera_Video_Start(void);
void  UCamera_Video_Stop(void);


int Ucamera_Audio_Regesit_CB(struct Ucamera_Audio_CB_Func *a_func);

int Ucamera_Audio_Get_Frame(struct Ucamera_Audio_Frame *frame);

int Ucamera_Audio_Release_Frame(struct Ucamera_Audio_Frame *frame);

int Ucamera_Audio_Start(void);

int Ucamera_DeInit(void);

int Ucamera_Hid_Init(void);
#endif /* _USBCAMERA_H_ */
