#!/bin/bash

set -ex

if [ $# != 1 ]; then
    echo "usage ./install.sh DESTDIR"
    exit 1
fi

DESTDIR=$(readlink -e $1)
echo $DESTDIR
if [ "x$DESTDIR" = "x" ]; then
	echo 'DESTDIR dose not exist'
	exit 1
fi

DESTDIR_DEVEL=$DESTDIR/../devel_rootfs

MYSCRIPT_DIR=$(cd $(dirname $0)>/dev/null 2>&1;pwd -P)
echo $MYSCRIPT_DIR


mkdir -p $DESTDIR/etc/lighttpd/
cp $MYSCRIPT_DIR/lighttpd.conf $DESTDIR/etc/lighttpd/
cp $MYSCRIPT_DIR/auth-conf.sh $DESTDIR/etc/lighttpd/

# install fcgi config files
mkdir -p $DESTDIR/etc/lighttpd/conf-available
mkdir -p $DESTDIR/opt/fcgi/cgi-bin/
capability_service_conf_files="\
16-apiinfo.conf \
16-cpuinfo.conf \
16-gpuinfo.conf \
16-meminfo.conf \
"

if [ "x$CONFIG_RESTFUL_CAPABILITY" = "xy" ]; then
    for conf in ${capability_service_conf_files}; do
        cp $MYSCRIPT_DIR/info/cpp/$conf $DESTDIR/etc/lighttpd/conf-available/
    done
fi


if [ "x$CONFIG_RESTFUL_SAMPLE" = "xy" ]; then
    for conf in `find . -name "16-*.conf"`; do
        cp $conf $DESTDIR/etc/lighttpd/conf-available/
    done

    for pydir in `find . -type d -name "python" -not -path "./build/*"`; do
        sample_pydir="$pydir"
        if [ -e $sample_pydir/fcgi_*.py ]; then
            cp $sample_pydir/fcgi_*.py $DESTDIR/opt/fcgi/cgi-bin/
        fi
    done

    if [ -d "$MYSCRIPT_DIR/asr/python/" ]; then
        mkdir -p $DESTDIR/opt/fcgi/cgi-bin/asr/
        mkdir -p $DESTDIR/opt/fcgi/cgi-bin/asr/ctcdecode_numpy/
        cp -rf $MYSCRIPT_DIR/asr/python/asr_utils $DESTDIR/opt/fcgi/cgi-bin/asr/
    fi
fi

# enable default services
###############################################################################


mkdir -p $DESTDIR/etc/lighttpd/conf-enabled
pushd $DESTDIR/etc/lighttpd/conf-enabled

enabled_list="\
${capability_service_conf_files} \
16-classification.conf \
16-classification-tf.conf \
16-face-detection.conf \
16-facial-landmark.conf \
16-digitalnote-py.conf \
16-tts-py.conf \
16-policy-py.conf \
16-policy.conf \
16-streaming.conf \
16-smartphoto.conf \
"
for conf in $enabled_list; do
    if [ -f $DESTDIR/etc/lighttpd/conf-available/$conf ]; then
        ln -sf ../conf-available/$conf
    fi
done
popd

cp -f $MYSCRIPT_DIR/../../health-monitor/health-monitor/service_runtime_health_monitor.proto ./
mkdir -p $MYSCRIPT_DIR/build
pushd $MYSCRIPT_DIR/build

source /opt/intel/openvino_2022/setupvars.sh
cmake -DCMAKE_FIND_ROOT_PATH=$DESTDIR_DEVEL ..
make -j$(nproc --all) VERBOSE=1
make DESTDIR=$DESTDIR install/strip

popd

#rm -rf $MYSCRIPT_DIR/build
