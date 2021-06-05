/*
 * Ingenic IMP RTSPServer subsession equal to H265VideoFileServerMediaSubsession.
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 * Author: Zoro <yakun.li@ingenic.com>
 */

#include "H265VideoServerMediaSubsession.hh"
#include "H265VideoRTPSink.hh"
#include "H265VideoStreamSource.hh"
#include "H265VideoStreamFramer.hh"
#include "H265VideoStreamDiscreteFramer.hh"
#include "ByteStreamFileSource.hh"
#include "VideoInput.hh"
#include "imp-common.hh"

//extern int g_VideoWidth;
//extern int g_VideoHeight;

H265VideoServerMediaSubsession*
H265VideoServerMediaSubsession
::createNew(UsageEnvironment& env, VideoInput& videoInput, unsigned estimatedBitrate) {
	return new H265VideoServerMediaSubsession(env, videoInput, estimatedBitrate);
}

H265VideoServerMediaSubsession
::H265VideoServerMediaSubsession(UsageEnvironment& env, VideoInput& videoInput, unsigned estimatedBitrate)
	: OnDemandServerMediaSubsession(env, True/*reuse the first source*/),
	  fAuxSDPLine(NULL), fDoneFlag(0), fDummyRTPSink(NULL),
	  fVideoInput(videoInput) {
	fEstimatedKbps = (estimatedBitrate + 500)/1000;
}

H265VideoServerMediaSubsession
::~H265VideoServerMediaSubsession() {
  delete[] fAuxSDPLine;
}

static void afterPlayingDummy(void* clientData) {
  H265VideoServerMediaSubsession* subsess = (H265VideoServerMediaSubsession*)clientData;
  subsess->afterPlayingDummy1();
}

void H265VideoServerMediaSubsession::afterPlayingDummy1() {
  // Unschedule any pending 'checking' task:
  envir().taskScheduler().unscheduleDelayedTask(nextTask());
  // Signal the event loop that we're done:
  setDoneFlag();
}

static void checkForAuxSDPLine(void* clientData) {
  H265VideoServerMediaSubsession* subsess = (H265VideoServerMediaSubsession*)clientData;
  subsess->checkForAuxSDPLine1();
}

void H265VideoServerMediaSubsession::checkForAuxSDPLine1() {
  char const* dasl;
  if (fAuxSDPLine != NULL) {
    // Signal the event loop that we're done:
    setDoneFlag();
  } else if (fDummyRTPSink != NULL && (dasl = fDummyRTPSink->auxSDPLine()) != NULL) {
    fAuxSDPLine = strDup(dasl);
    fDummyRTPSink = NULL;

    // Signal the event loop that we're done:
    setDoneFlag();

  } else if (!fDoneFlag) {
    // try again after a brief delay:
    int uSecsToDelay = 100000; // 100 ms
    nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay,
			      (TaskFunc*)checkForAuxSDPLine, this);
  }
}

char const* H265VideoServerMediaSubsession::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) {
  if (fAuxSDPLine != NULL) return fAuxSDPLine; // it's already been set up (for a previous client)

  if (fDummyRTPSink == NULL) { // we're not already setting it up for another, concurrent stream
    // Note: For H265 video files, the 'config' information ("profile-level-id" and "sprop-parameter-sets") isn't known
    // until we start reading the file.  This means that "rtpSink"s "auxSDPLine()" will be NULL initially,
    // and we need to start reading data from our file until this changes.
    fDummyRTPSink = rtpSink;

    // Start reading the file:
    fDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);

    // Check whether the sink's 'auxSDPLine()' is ready:
    checkForAuxSDPLine(this);
  }
  envir().taskScheduler().doEventLoop(&fDoneFlag);
  return fAuxSDPLine;
}

FramedSource* H265VideoServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
	estBitrate = fEstimatedKbps;

	// Create a framer for the Video Elementary Stream:
	return H265VideoStreamDiscreteFramer::createNew(envir(), fVideoInput.videoSource());
}

RTPSink* H265VideoServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock,
		   unsigned char rtpPayloadTypeIfDynamic,
		   FramedSource* /*inputSource*/) {
	OutPacketBuffer::maxSize = g_VideoWidth * g_VideoHeight * 2;
	return H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}
