/*
 * Ingenic IMP RTSPServer VideoInput
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 * Author: Zoro <yakun.li@ingenic.com>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <sys/ioctl.h>

#include <imp/imp_log.h>
#include <imp/imp_common.h>
#include <imp/imp_system.h>
#include <imp/imp_utils.h>
#include <imp/imp_framesource.h>
#include <imp/imp_encoder.h>
#include <imp/imp_isp.h>

#include "VideoInput.hh"
#include "imp-common.hh"
#include "H264VideoStreamSource.hh"

#define TAG 						"sample-RTSPServer"

pthread_t VideoInput::ispTuneTid = -1;

int gconf_Main_VideoWidth = SENSOR_WIDTH;
int gconf_Main_VideoHeight = SENSOR_HEIGHT;

IMPEncoderProfile gconf_mainPayLoad =  IMP_ENC_PROFILE_HEVC_MAIN;
IMPEncoderRcMode gconf_defRC = IMP_ENC_RC_MODE_CAPPED_QUALITY;

Boolean VideoInput::fpsIsOn[MAX_STREAM_CNT] = {False, False};
Boolean VideoInput::fHaveInitialized = False;
double VideoInput::gFps[MAX_STREAM_CNT] = {0.0, 0.0};
double VideoInput::gBitRate[MAX_STREAM_CNT] = {0.0, 0.0};

static int framesource_init(IMPFSChnAttr *imp_chn_attr)
{
	int ret;

	/* create channel i*/
	ret = IMP_FrameSource_CreateChn(0, imp_chn_attr);
	if(ret < 0){
		IMP_LOG_ERR(TAG, "IMP_FrameSource_CreateChn(chn%d) error!\n", 0);
		return -1;
	}

	ret = IMP_FrameSource_SetChnAttr(0, imp_chn_attr);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_FrameSource_SetChnAttr(%d) error: %d\n", ret, 0);
		return -1;
	}

	/* Check channel i attr */
	IMPFSChnAttr imp_chn_attr_check;
	ret = IMP_FrameSource_GetChnAttr(0, &imp_chn_attr_check);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_FrameSource_GetChnAttr(%d) error: %d\n", ret, 0);
		return -1;
	}

	ret = IMP_ISP_EnableTuning();
	if(ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_ISP_EnableTuning error\n");
		return -1;
	}

	return 0;
}

static int encoder_param_defalt(IMPEncoderChnAttr *chnAttr, IMPEncoderProfile profile, IMPEncoderRcMode rcMode,
        int w, int h, int outFrmRateNum, int outFrmRateDen, int outBitRate)
{
    int ret = 0;
    IMPEncoderEncType encType = (IMPEncoderEncType)(profile >> 24);

    if ((encType < IMP_ENC_TYPE_AVC) || (encType > IMP_ENC_TYPE_JPEG)) {
        IMP_LOG_ERR(TAG, "unsupported encode type:%d, we only support avc, hevc and jpeg type\n", encType);
        return -1;
    }

    if (encType == IMP_ENC_TYPE_JPEG) {
        rcMode = IMP_ENC_RC_MODE_FIXQP;
    }

    if ((rcMode < IMP_ENC_RC_MODE_FIXQP) || (rcMode > IMP_ENC_RC_MODE_CAPPED_QUALITY)) {
        IMP_LOG_ERR(TAG, "unsupported rcmode:%d, we only support fixqp, cbr, vbr, capped vbr and capped quality\n", rcMode);
        return -1;
    }

    memset(chnAttr, 0, sizeof(IMPEncoderChnAttr));

    ret = IMP_Encoder_SetDefaultParam(chnAttr, profile, rcMode, w, h, outFrmRateNum, outFrmRateDen, outFrmRateNum * 2 / outFrmRateDen,
            ((rcMode == IMP_ENC_RC_MODE_CAPPED_VBR) || (rcMode == IMP_ENC_RC_MODE_CAPPED_QUALITY)) ? 3 : 1,
            (rcMode == IMP_ENC_RC_MODE_FIXQP) ? 35 : -1, outBitRate);
    if (ret < 0) {
        IMP_LOG_ERR(TAG, "IMP_Encoder_SetDefaultParam failed\n");
        return -1;
    }


    return 0;
}

static int encoder_init(int streamNum)
{
	int ret = 0;
        int  grpNum = 0;
	IMPEncoderChnAttr chnAttr;

	encoder_param_defalt(&chnAttr, gconf_mainPayLoad, gconf_defRC,gconf_Main_VideoWidth,gconf_Main_VideoHeight, SENSOR_FRAME_RATE_NUM, SENSOR_FRAME_RATE_DEN,BITRATE_720P_Kbs);

		/* Creat Encoder Group */
			ret = IMP_Encoder_CreateGroup(grpNum);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_Encoder_CreateGroup(%d) error: %d\n", grpNum, ret);
				return -1;
			}

		/* Create Channel */
		ret = IMP_Encoder_CreateChn(streamNum, &chnAttr);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "IMP_Encoder_CreateChn(0) error: %d\n", ret);
			return -1;
		}

		/* Resigter Channel */
	    ret = IMP_Encoder_RegisterChn(grpNum, streamNum);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "IMP_Encoder_RegisterChn(%d, 0) error: %d\n", grpNum, 0, ret);
			return -1;
		}

	return 0;
}

static int ImpSystemInit()
{
	int ret = 0;
	IMPSensorInfo sensor_info;
	
	memset(&sensor_info, 0, sizeof(IMPSensorInfo));
	memcpy(sensor_info.name, SENSOR_NAME, sizeof(SENSOR_NAME));
	sensor_info.cbus_type = SENSOR_CUBS_TYPE;
	memcpy(sensor_info.i2c.type, SENSOR_NAME, sizeof(SENSOR_NAME));
	sensor_info.i2c.addr = SENSOR_I2C_ADDR;


	IMP_LOG_DBG(TAG, "ImpSystemInit start\n");
       
	ret = IMP_ISP_Open();
	if(ret < 0){
		IMP_LOG_ERR(TAG, "failed to open ISP\n");
		return -1;
	}

	ret = IMP_ISP_AddSensor(&sensor_info);
	if(ret < 0){
		IMP_LOG_ERR(TAG, "failed to AddSensor\n");
		return -1;
	}

	ret = IMP_ISP_EnableSensor();
	if(ret < 0){
		IMP_LOG_ERR(TAG, "failed to EnableSensor\n");
		return -1;
	}

	IMP_System_Init();

	ret = IMP_ISP_EnableTuning();
	if(ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_ISP_EnableTuning error\n");
		return -1;
	}

	ret = IMP_ISP_Tuning_SetSensorFPS(SENSOR_FRAME_RATE_NUM, SENSOR_FRAME_RATE_DEN);
	if (ret < 0){
		IMP_LOG_ERR(TAG, "failed to set sensor fps\n");
		return -1;
        }	
        IMP_LOG_DBG(TAG, "ImpSystemInit success\n");

	return 0;
}

static int imp_init(void)
{
	int ret;

	/* System init */
	ret = ImpSystemInit();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_System_Init() failed\n");
		return -1;
	}

	/* FrameSource init */
	IMPFSChnAttr imp_chn_attr[2];
	imp_chn_attr[0].pixFmt = PIX_FMT_NV12;
	imp_chn_attr[0].outFrmRateNum = SENSOR_FRAME_RATE_NUM;
	imp_chn_attr[0].outFrmRateDen = SENSOR_FRAME_RATE_DEN;
	imp_chn_attr[0].nrVBs = 2;
	imp_chn_attr[0].type = FS_PHY_CHANNEL;

	imp_chn_attr[0].crop.enable = 1;
	imp_chn_attr[0].crop.top = 0;
	imp_chn_attr[0].crop.left = 0;

	imp_chn_attr[0].crop.width = gconf_Main_VideoWidth;
	imp_chn_attr[0].crop.height = gconf_Main_VideoHeight;

	imp_chn_attr[0].scaler.enable = 0;

	imp_chn_attr[0].picWidth = gconf_Main_VideoWidth;
	imp_chn_attr[0].picHeight = gconf_Main_VideoHeight;

	ret = framesource_init(imp_chn_attr);
        if (ret < 0) {
		IMP_LOG_ERR(TAG, "FrameSource init failed\n");
		return -1;
	}

	/* Encoder init */
	ret = encoder_init(1);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "Encoder init failed\n");
		return -1;
	}

	/* Bind */
	IMPCell framesource_chn0_output0 = {DEV_ID_FS, 0, 0};
	IMPCell encoder0 = {DEV_ID_ENC, 0, 0};

	ret = IMP_System_Bind(&framesource_chn0_output0, &encoder0);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_System_Bind(&framesource_chn0_output0, &encoder0) failed...\n");
		return ret;
	}

	IMP_FrameSource_SetFrameDepth(0, 0);

	ret = IMP_FrameSource_EnableChn(0);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_FrameSource_EnableChn(%d) error: %d\n", ret, 0);
		return -1;
	}

	return 0;
}

static inline int close_stream_file(int fd)
{
	return close(fd);
}

void *VideoInput::ispAutoTuningThread(void *p){
    int ret;	
    ret = uvc_system_init();
	if (ret < 0) {
		printf("%s: %d\n", __func__, __LINE__);
		return false;
	}

	return 0; 

}

VideoInput* VideoInput::createNew(UsageEnvironment& env, int streamNum) {
    if (!fHaveInitialized) {
		if (!initialize(env)) {
			printf("%s: %d\n", __func__, __LINE__);
			return NULL;
		}
		fHaveInitialized = True;
    }

	VideoInput *videoInput = new VideoInput(env, streamNum);

    return videoInput;
}

VideoInput::VideoInput(UsageEnvironment& env, int streamNum)
	: Medium(env), fVideoSource(NULL), fpsIsStart(False), fontIsStart(False),
	  osdIsStart(False), osdStartCnt(0), nrFrmFps(0),
	  totalLenFps(0), startTimeFps(0), streamNum(streamNum), scheduleTid(-1),
	  orgfrmRate(SENSOR_FRAME_RATE_NUM), hasSkipFrame(false),
	  curPackIndex(0), requestIDR(false) {
}

VideoInput::~VideoInput() {
}

extern "C" {
	int IMP_FrameSource_EnableChnUndistort(int chnNum, void *handle);
	int IMP_FrameSource_DisableChnUndistort(int chnNum);
}

bool VideoInput::initialize(UsageEnvironment& env) {
	int ret;

	ret = imp_init();
	if (ret < 0) {
		return false;
	}
    
    ret = pthread_create(&ispTuneTid, NULL, VideoInput::ispAutoTuningThread, NULL);
	if (ret < 0) {
        printf("pthread_create err\n");
		return false;
	}
    
    

	return true;
}

FramedSource* VideoInput::videoSource() {
	IMP_Encoder_FlushStream(streamNum);
	fVideoSource = new H264VideoStreamSource(envir(), *this);
	return fVideoSource;
}

int VideoInput::getStream(void* to, unsigned int* len, struct timeval* timestamp, unsigned fMaxSize) {
	int ret;
	unsigned int stream_len = 0;
	unsigned int  remSize = 0;

	if (curPackIndex == 0) {
		ret = IMP_Encoder_GetStream(streamNum, &bitStream, 1);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "IMP_Encoder_GetStream() failed\n");
			return -1;
		}
	}

	if (requestIDR) {
		if (bitStream.packCount == 1) {
			IMP_Encoder_ReleaseStream(streamNum, &bitStream);
			goto out;
		} else {
			requestIDR = false;
		}
	}

	stream_len = bitStream.pack[curPackIndex].length - 4;

	if (stream_len > fMaxSize) {
		IMP_LOG_WARN(TAG, "drop stream: length=%u, fMaxSize=%d\n", stream_len, fMaxSize);
		stream_len = 0;
		curPackIndex = 0;
		IMP_Encoder_ReleaseStream(streamNum, &bitStream);
		requestIDR = true;
		goto out;
	}
	remSize = bitStream.streamSize - bitStream.pack[curPackIndex].offset;
	if (remSize < bitStream.pack[curPackIndex].length) {
		memcpy((void*)((unsigned int)to),
		   (void *)(bitStream.virAddr + bitStream.pack[curPackIndex].offset + 4), remSize - 4);
		memcpy((void*)((unsigned int)to + remSize -4), (void *)bitStream.virAddr, (bitStream.pack[curPackIndex].length - remSize));
	}else {
		memcpy((void*)((unsigned int)to),
		   (void *)(bitStream.virAddr + bitStream.pack[curPackIndex].offset + 4), stream_len);
	}

	if (curPackIndex < bitStream.packCount - 1)
		curPackIndex++;
	else if (curPackIndex == bitStream.packCount - 1)
		curPackIndex = 0;

	if (curPackIndex == 0) {
		IMP_Encoder_ReleaseStream(streamNum, &bitStream);
	}

	gettimeofday(timestamp, NULL);

	if (fpsIsOn[streamNum]) {
		if (fpsIsStart == False) {
			fpsIsStart = True;
			nrFrmFps = 0;
			totalLenFps = 0;
			startTimeFps = IMP_System_GetTimeStamp();
		}
		totalLenFps += stream_len;
		if (curPackIndex == 0)
			nrFrmFps++;
		if ((nrFrmFps > 0) && ((nrFrmFps % (SENSOR_FRAME_RATE_DEN * 2 / SENSOR_FRAME_RATE_DEN)) == 0)) {
			uint64_t now = IMP_System_GetTimeStamp();
			uint64_t elapsed = now - startTimeFps;

			double fps = nrFrmFps * 1000000. / (elapsed > 0 ? elapsed : 1);
			double kbitrate = (double)totalLenFps * 8  * 1000. / (elapsed > 0 ? elapsed : 1);

			//printf("\rstreamNum[%d]:FPS: %0.2f,Bitrate: %0.2f(kbps)", streamNum, (fps<19.8)?fps+0.5:fps, kbitrate);
			printf("\rstreamNum[%d]:FPS: %0.2f,Bitrate: %0.2f(kbps)", streamNum, fps, kbitrate);
			fflush(stdout);
#ifdef SUPPORTCLASSOSD
			if(gFPSShowEn == true) {
				char str[50] = {0};
				//sprintf(str, "%s%0.2f%s %s%0.2f%s", "FPS:", (fps<19.8)?fps+0.5:fps, "/s", "RATE:", kbitrate/8, "KB");
				sprintf(str, "%s%0.2f%s %s%0.2f%s", "FPS:", fps, "/s", "RATE:", kbitrate/8, "KB");
				if(streamNum == 0)
					gOSDFPSShow->StringWrite(str);
				else if(streamNum == 1)
					gOSDFPSShow_sec->StringWrite(str);
			}
#endif
			gFps[streamNum] = fps;
			gBitRate[streamNum] = kbitrate;

			startTimeFps = now;
			totalLenFps = 0;
			nrFrmFps = 0;
		}
	} else if (fpsIsStart) {
		fpsIsStart = False;
	}

out:
	*len = stream_len;

	return 0;
}

int VideoInput::pollingStream(void)
{
	int ret;

	ret = IMP_Encoder_PollingStream(streamNum, 2000);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "chnNum:%d, Polling stream timeout\n", streamNum);
		return -1;
	}

	return 0;
}

int VideoInput::streamOn(void)
{

	IMP_Encoder_RequestIDR(streamNum);
	requestIDR = true;
    printf("---------->VideoInput::streamOn<----------\n");

	int ret = IMP_Encoder_StartRecvPic(streamNum);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_Encoder_StartRecvPic(%d) failed\n", streamNum);
		return -1;
	}

	return 0;
}

int VideoInput::streamOff(void)
{
	int ret;
    printf("---------->VideoInput::streamOff<----------\n");

	if (curPackIndex != 0)
		IMP_Encoder_ReleaseStream(streamNum, &bitStream);

	ret = IMP_Encoder_StopRecvPic(streamNum);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_Encoder_StopRecvPic(%d) failed\n", streamNum);
		return -1;
	}

	return 0;
}
