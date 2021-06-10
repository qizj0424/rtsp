/*
 * Ingenic IMP RTSPServer MAIN.
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 * Author: Zoro <yakun.li@ingenic.com>
 */

#include <BasicUsageEnvironment.hh>
#include "H264VideoServerMediaSubsession.hh"
#include "H265VideoServerMediaSubsession.hh"
#include "VideoInput.hh"
#include "imp-common.hh"
#include "RTSPServer.hh"
#include <stdio.h>

portNumBits rtspServerPortNum = 8554;
char* streamDescription = strDup("RTSP/RTP stream from Ingenic Media");

extern IMPEncoderProfile gconf_mainPayLoad;

#define BR_AT_1M 2000000 //1Mbps at 1M pixel

static inline int calc_bit_rate(int width, int height)
{
	return BR_AT_1M * width / 1280 * height / 720;
}
int main(int argc, char** argv)
{
#if 1
	TaskScheduler* scheduler = BasicTaskScheduler::createNew();
	UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);

	VideoInput* videoInput = VideoInput::createNew(*env, 1);
	if (videoInput == NULL) {
		printf("Video Input init failed %s: %d\n", __func__, __LINE__);
		exit(1);
	}

	// Create the RTSP server:
	RTSPServer* rtspServer = NULL;
	// Normal case: Streaming from a built-in RTSP server:
	rtspServer = RTSPServer::createNew(*env, rtspServerPortNum, NULL);
	if (rtspServer == NULL) {
		printf("Failed to create RTSP server %s: %d\n", __func__, __LINE__);
		exit(1);
	}

	ServerMediaSession* sms_main =
		ServerMediaSession::createNew(*env, "main", NULL, streamDescription, False);


    if ((gconf_mainPayLoad >> 24) == IMP_ENC_TYPE_AVC) {
        sms_main->addSubsession(H264VideoServerMediaSubsession::createNew(sms_main->envir(), *videoInput, calc_bit_rate(g_VideoWidth, g_VideoHeight)));
    } else {
        sms_main->addSubsession(H265VideoServerMediaSubsession::createNew(sms_main->envir(), *videoInput, calc_bit_rate(g_VideoWidth, g_VideoHeight)));
                                //H265VideoServerMediaSubsession::createNew(UsageEnvironment&, VideoInput&, unsigned int)'
    }

	rtspServer->addServerMediaSession(sms_main);
	char *url = rtspServer->rtspURL(sms_main);
	printf("Play this video stream using the URL: %s\n", url);
	delete[] url;

	// Begin the LIVE555 event loop:
	env->taskScheduler().doEventLoop(); // does not return

#endif
	return 0; //only to prevent compiler warning
}
