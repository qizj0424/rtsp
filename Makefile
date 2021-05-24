
SRC_FILE = H264VideoServerMediaSubsession.o H264VideoStreamSource.o RTSPServer.o VideoInput.o
EXT_LIB = -L./live555/prebuilt/lib/uclibc/

SRC_DIR = $(shell ls -R | grep ":")
INCLUDE_DIR = $(subst .,-I.,$(SRC_DIR))
INCLUDE = $(subst :,,$(INCLUDE_DIR))

LINK_TARGET = Tseries_RTSP_demo

CROSS_COMPILE=mips-linux-uclibc-gnu-
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
STRIP=$(CROSS_COMPILE)strip
LD=$(CROSS_COMPILE)ld

CFLAGS= -Os -Wall -Werror \
	./sdk/lib/uclibc/libimp.a \
	./sdk/lib/uclibc/libsysutils.a \
	./sdk/lib/uclibc/libalog.a \
	-lliveMedia -lBasicUsageEnvironment \
	-lUsageEnvironment -lgroupsock -lpthread -lrt

ODIR=obj
OBJ  = $(SRC_FILE:.cpp=.o)
OBJECTS = $(addprefix $(ODIR)/,$(notdir $(OBJ)))

.PHONY: all
all: mk_obj $(LINK_TARGET)

mk_obj:
	mkdir -p $(ODIR)

$(LINK_TARGET): mk_obj $(OBJ)
	$(CXX) -o $(ODIR)/$@ $(OBJECTS) $(INCLUDE) $(EXT_LIB) $(CFLAGS)
	$(STRIP) $(ODIR)/$@

%.o:%.cpp
	$(CXX) $(CFLAGS) $(INCLUDE) -I$(ODIR) -c -o $(ODIR)/$(notdir $@) $<

clean:
	rm $(ODIR) -rf
