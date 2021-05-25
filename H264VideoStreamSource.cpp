/*
 * Ingenic IMP RTSPServer H264VideoStreamSource
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 * Author: Zoro <yakun.li@ingenic.com>
 */

#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#include "H264VideoStreamSource.hh"

static void* PollingThread(void *p);

H264VideoStreamSource*
H264VideoStreamSource::createNew(UsageEnvironment& env, VideoInput& input)
{
	H264VideoStreamSource* newSource = new H264VideoStreamSource(env, input);

	return newSource;
}

H264VideoStreamSource::H264VideoStreamSource(UsageEnvironment& env, VideoInput& input)
	: FramedSource(env), eventTriggerId(0), fInput(input) {
	eventTriggerId = envir().taskScheduler().createEventTrigger(incomingDataHandler);
	sem_init(&sem, 0, 0);

	int error;
	error = pthread_create(&polling_tid, NULL, PollingThread, this);
	if (error) {
		envir() << "PollingThread create error:" << strerror(errno);
        abort();
	} else {
        fInput.streamOn();
    }
}

H264VideoStreamSource::~H264VideoStreamSource() {
	pthread_cancel(polling_tid);
	pthread_join(polling_tid, NULL);
	envir().taskScheduler().deleteEventTrigger(eventTriggerId);
	eventTriggerId = 0;
	sem_destroy(&sem);
	fInput.streamOff();
	fInput.fVideoSource = NULL;
}

static void* PollingThread(void *p) {
	H264VideoStreamSource *vss = (H264VideoStreamSource *)p;

	return vss->PollingThread1();
}

void* H264VideoStreamSource::PollingThread1() {
	while (1) {
		sem_wait(&sem);
		if (!fInput.skipPolling()) {
			//int old_state, ret;
//			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);
			fInput.pollingStream();
//			pthread_setcancelstate(old_state, NULL);
		}

		if (eventTriggerId != 0) {
			envir().taskScheduler().triggerEvent(eventTriggerId, this);
		}
	}

	return NULL;
}

void H264VideoStreamSource::incomingDataHandler(void* clientData) {
	((H264VideoStreamSource*)clientData)->incomingDataHandler1();
}

void H264VideoStreamSource::incomingDataHandler1() {
	fInput.getStream((void *)fTo, &fFrameSize, &fPresentationTime, fMaxSize);
	afterGetting(this);
}

int pause_stream = 0;

void H264VideoStreamSource::doGetNextFrame() {
	if (pause_stream) {
		afterGetting(this);
		return;
	} else {
		sem_post(&sem);
	}
}
