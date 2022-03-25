# Copyright (C) 2021 Intel Corporation

DESTDIR:=/
CC=gcc
CFLAGS=-Wall -g -O2 -fPIC -I. \
       -fstack-protector-all -D_FORTIFY_SOURCE=2 -O2 \
       $(shell pkg-config --cflags gstreamer-1.0) \
       $(shell pkg-config --cflags dl-streamer) \
       -I thirdparty  \
       -I /opt/intel/openvino/opencv/include \
       -I ../inference-runtime/release/include
CXXFLAGS=$(CFLAGS) -std=c++17
LIBS=$(shell pkg-config --libs gstreamer-1.0)
LDFLAGS=-Wl,-z,now -Wl,-z,relro \
       -L ../inference-runtime/release/lib

lib=libccai_stream.so
lib_SRCS=ccai_stream.c plugin.c
lib_OBJS=$(patsubst %.c,%.o,$(lib_SRCS))

test=ccai_stream_test
test_SRCS=test.c
test_OBJS=$(patsubst %.c,%.o,$(test_SRCS))

launcher_plugin=plugins/general_launcher.so
launcher_plugin_SRCS=general_launcher.c
launcher_plugin_OBJS=$(patsubst %.c,%.o,$(launcher_plugin_SRCS))

backgroundblur_plugin=plugins/backgroundblur.so
backgroundblur_plugin_SRCS=backgroundblur.cpp
backgroundblur_plugin_OBJS=$(patsubst %.cpp,%.o,$(backgroundblur_plugin_SRCS))

emotion_plugin=plugins/emotion.so
emotion_plugin_SRCS=emotion.cpp
emotion_plugin_OBJS=$(patsubst %.cpp,%.o,$(emotion_plugin_SRCS))

pose_plugin=plugins/pose.so
pose_plugin_SRCS=pose.cpp hpe_eHRNet.cpp thirdparty/associative_embedding_decoder.cpp thirdparty/kuhn_munkres.cpp
pose_plugin_OBJS=$(patsubst %.cpp,%.o,$(pose_plugin_SRCS))

SRCS=$(lib_SRCS) \
     $(test_SRCS) \
     $(launcher_plugin_SRCS) \
     $(backgroundblur_plugin_SRCS) \
     $(emotion_plugin_SRCS) \
     $(pose_plugin_SRCS)

all: .depend $(lib) \
	$(test) \
	$(launcher_plugin) \
	$(backgroundblur_plugin) \
	$(emotion_plugin) \
	$(pose_plugin)

$(lib):$(lib_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(lib_OBJS) -shared $(LIBS)

$(test):$(test_OBJS) $(lib)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(test_OBJS) -lccai_stream -L. $(LIBS)

$(launcher_plugin):$(launcher_plugin_OBJS) $(lib)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(launcher_plugin_OBJS) \
		-shared -lccai_stream -L. $(LIBS) \
		-ljson-c

$(backgroundblur_plugin):$(backgroundblur_plugin_OBJS) $(lib)
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $(backgroundblur_plugin_OBJS) \
		-shared -lccai_stream -L. $(LIBS) \
		-L/opt/intel/openvino/opencv/lib \
		-lopencv_core -lopencv_imgproc \
		-linferservice -lpython3.8

$(emotion_plugin):$(emotion_plugin_OBJS) $(lib)
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $(emotion_plugin_OBJS) \
		-shared -lccai_stream -L. $(LIBS) \
		-L/opt/intel/openvino/opencv/lib \
		-lopencv_core -lopencv_imgproc \
		-linferservice -lpython3.8 \
		$(shell pkg-config --libs gstreamer-video-1.0)

$(pose_plugin):$(pose_plugin_OBJS) $(lib)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(pose_plugin_OBJS) \
		-shared -lccai_stream -L. $(LIBS) \
		-L/opt/intel/openvino/opencv/lib \
		-linferservice -lopenvinoentry \
		-lopencv_core -lopencv_imgproc -lopencv_imgcodecs \
		$(shell pkg-config --libs gstreamer-video-1.0) \
		-lrt -lpthread -lstdc++ -lpython3.8

%o:%c
	$(CC) $(CFLAGS) -c $< -o $@
%o:%cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
.depend: $(SRCS)
	$(CC) $(CFLAGS) -MM $^ > "$@"
include .depend

clean:
	rm -f *.o
	rm -f thirdparty/*.o
	rm -f *.so
	rm -f plugins/*.so
	rm -f $(test)
	rm -f .depend

lib_install_DIR=$(DESTDIR)/usr/lib/x86_64-linux-gnu/
plugin_install_DIR=$(DESTDIR)/usr/lib/ccai_stream/plugins
include_DIR=$(DESTDIR)/usr/include
install: all
	mkdir -p $(lib_install_DIR)
	mkdir -p $(plugin_install_DIR)
	mkdir -p $(include_DIR)

	strip ./libccai_stream.so
	strip ./plugins/*.so

	cp -a ./libccai_stream.so $(lib_install_DIR)
	cp -a ./plugins/*.so $(plugin_install_DIR)
	cp -a ./plugins/*.conf $(plugin_install_DIR)

	cp ./ccai_stream_plugin.h $(include_DIR)
	cp ./ccai_stream_utils.h $(include_DIR)
	cp ./ccai_stream.h $(include_DIR)
