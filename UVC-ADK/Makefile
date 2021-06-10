ifeq ($(LIBTYPE), muclibc)
	CROSS_COMPILE := mips-linux-uclibc-gnu-
else
	CROSS_COMPILE := mips-linux-gnu-
endif

PLATFORM_TYPE ?= Linux

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
AR := $(CROSS_COMPILE)ar

ifeq ($(PLATFORM_TYPE), Linux)
	CFLAGS := -lstdc++ -Wall -fPIC
endif

ifeq ($(LIBTYPE), muclibc)
	LIBS :=  -L./ucamcore/lib/uclibc -lusbcamera
else
	LIBS :=  -L./ucamcore/lib/glibc -lusbcamera
endif

INCLUDE := -I./include/

OBJS := ./adk_usbcamera.o

TARGET := libadk_usbcamera.a

All: $(TARGET)

%.o:%.c
	$(CC) $(CFLAGS) $(INCLUDE) $(LIBS) -o $@ -c $^

$(TARGET): $(OBJS)
	$(LD) -r -Bstatic $^ -o $@ $(LIBS)

clean:
	rm -f $(TARGET) $(OBJS)
