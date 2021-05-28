

Target = UVC_NET_camera 
COPY_PATH = ~/nfs_shared/

CROSS_COMPILE ?= mips-linux-gnu-
COMPILE_TYPE ?= uclibc
# COMPILE_TYPE ?= glibc

CC = $(CROSS_COMPILE)gcc
CXX= $(CROSS_COMPILE)g++
LD = $(CROSS_COMPILE)ld
AR = $(CROSS_COMPILE)ar cr
STRIP = $(CROSS_COMPILE)strip
HOST := T31

export HOST COMPILE_TYPE

CFLAGS = -O2 -Wall -march=mips32r2

ifeq ($(HOST), T31)
	SDK_DIR := t31_sdk
	CFLAGS += -DT31
endif

ifeq ($(HOST), T21)
	SDK_DIR := t21_sdk
	CFLAGS += -DT21
endif

ifeq ($(HOST), T20)
	SDK_DIR := t20_sdk
	CFLAGS += -DT20
endif

ifeq ($(COMPILE_TYPE), uclibc)
	CFLAGS += -muclibc
	LDFLAG += -muclibc
endif

sources_file = $(wildcard *.cpp)
object_file = $(patsubst %.cpp,%.o,$(sources_file))
include_file = $(wildcard *.h)

INCLUDES = -I./ \
		-I./$(SDK_DIR)/include \
		-I./uvclib/include \
		-I./live555/prebuilt/include/BasicUsageEnvironment \
		-I./live555/prebuilt/include/groupsock \
		-I./live555/prebuilt/include/liveMedia \
		-I./live555/prebuilt/include/UsageEnvironment

LIBS := -L./$(SDK_DIR)/lib/$(COMPILE_TYPE) -limp -lalog -lsysutils \
		-L./uvclib/lib/$(COMPILE_TYPE) -lusbcamera \
		-L./live555/prebuilt/lib/$(COMPILE_TYPE) -lliveMedia -lgroupsock \
			-lBasicUsageEnvironment -lUsageEnvironment 

LIBS += -lpthread -lm -lrt -ldl



$(Target) : $(object_file)
	$(CXX) $^ $(CFLAGS) ${LIBS} -o $@

%.o : %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@ $(INCLUDES)

.PHONY:clean cleanall cp

clean:
	-rm -rf *.o

cleanall: clean
	-rm -rf $(Target)

cp:
	cp ./$(Target) $(COPY_PATH)
