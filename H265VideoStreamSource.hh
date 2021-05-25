/*
 * Ingenic IMP RTSPServer H265VideoStreamSource equal to H265VideoStreamFramer.
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 * Author: Zoro <yakun.li@ingenic.com>
 */

#ifndef H265VIDEOSTREAMSOURCE_HH
#define H265VIDEOSTREAMSOURCE_HH

#include <pthread.h>
#include <semaphore.h>
#include "FramedSource.hh"
#include "VideoInput.hh"

class H265VideoStreamSource: public FramedSource {
public:
  static H265VideoStreamSource* createNew(UsageEnvironment& env, VideoInput& input);
  void* PollingThread1();
  H265VideoStreamSource(UsageEnvironment& env, VideoInput& input);
  // called only by createNew()
  virtual ~H265VideoStreamSource();

public:
  EventTriggerId eventTriggerId;


private:
  static void incomingDataHandler(void* clientData);
  void incomingDataHandler1();
  virtual void doGetNextFrame();

private:
  pthread_t polling_tid;
  sem_t sem;
  VideoInput& fInput;

protected:
  Boolean isH265VideoStreamFramer() const { return True; }
  unsigned maxFrameSize()  const { return 150000; }
};

#endif

