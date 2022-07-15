# 12. Test cases and packages installation

## 12.1 Enabled services for testing

For making the deliverables unified but still easy to validate basic functions of CCAI framework, we disabled all sample services by default from version 1.0-210201 due to, on the current stage, no landed user cases from customers.
For testing purpose, to enable services, use these steps:

```
    #Create a folder on the host side under '/opt/intel/service_runtime'.
    $>sudo mkdir -p /opt/intel/service_runtime/rootfs/d/etc/runit/runsvdir/default
    $>sudo chown www-data.www-data /opt/intel/service_runtime/rootfs/d/etc/runit/runsvdir/default

    #Create the health monitor configuration file.
    $>sudo mkdir -p /opt/intel/service_runtime/rootfs/f/opt/health_monitor
    $>sudo bash -c 'cat > /opt/intel/service_runtime/rootfs/f/opt/health_monitor/config.yml << "EOF"
    daemon_targets:
    - lighttpd
    - policy_daemon
    grpc_targets:
    fcgi_targets:
    - fcgi_ocr.py
    - fcgi_ocr
    EOF'

    $>sudo chown -R www-data.www-data /opt/intel/service_runtime/rootfs/f/opt/health_monitor
```

replace/add/remove services in above "fcgi_targets:" section will replace/enable/disable related services from health_monitor list. For version 1.0-210201, now you can use 3 services as:

\- fcgi_ocr.py

\- fcgi_ocr

\- fcgi_tts.py **(Please noted, there is no fcgi_tts)**

\- fcgi_speech **(Please noted, there is no fcgi_speech.py)**

```
    #Restart container, and enable the services.
    $>sudo systemctl restart service-runtime
    $>docker exec -it service_runtime_container /bin/bash -c 'cd /etc/runit/runsvdir/default; for s in /etc/sv/*; do ln -sf $s; done'
```

**Remove those created folders under /opt/intel/service_runtime and restart the container, which would make any enabled services disabled again.**

```
    $>sudo rm -R /opt/intel/service_runtime/rootfs
```

## 12.2 High Level APIs test cases {#12.2}

Exposed high level APIs test cases which were provided in individual test script way, the usage of each case can be found in the following pages and the cases list are:

**(Most test cases have default input which are the preinstalled files under specific folders with the deb package installation. From the WW45'20 release, if you want to use your own input files like images (-i), text (-s), wav (-a), you now can pass input parameters to each test case.)**

### 12.2.1 For testing all provided API in a bunch

You can test all python implementations with existing test case set -
test-script/run_test_script.sh

Usage:

```
cd /opt/intel/service_runtime/test-script/
sudo ./run_test_script.sh
```



### 12.2.2 For testing python implementation of related REST APIs {#12.2.2}

test-script/test-demo/post_local_asr_py.py (the default input audio file("-a"):
how_are_you_doing.wav; the default inference device("-d"): GNA_AUTO)

Usage:

```
cd /opt/intel/service_runtime/test-script/
python3 ./test-demo/post_local_asr_py.py -a "AUDIO_FILE" -d "DEVICE"
```

Result:

```json
{
    "ret": 0,
    "msg": "ok",
    "data": {
        "text": "HOW ARE YOU DOING\n"
    },
    "time": 0.777
}

processing time is: 0.7873961925506592
```

test-script/test-demo/post_local_classfication_py.py (default input file is classfication.jpg if without input parameter )

Usage:

```
cd /opt/intel/service_runtime/test-script/
python3 ./test-demo/post_local_classfication_py.py -i "IMAGE_FILE"
```

Result:

```json
{
    "ret": 0,
    "msg": "ok",
    "data": {
        "tag_list": [
            {
                "tag_name": "sandal",
                "tag_confidence": 0.7865033745765686
            }
        ]
    },
    "time": 0.332
}

processing time is: 0.33770060539245605
```

test-script/test-demo/post_local_face_detection_py.py (default input file if without input parameter: face-detection-adas-0001.png)

Usage:

```
cd /opt/intel/service_runtime/test-script/
python3 ./test-demo/post_local_face_detection_py.py -i "IMAGE_FILE"
```

Result:

```json
{
    "ret": 0,
    "msg": "ok",
    "data": {
        "face_list": [
            {
                "x1": 611,
                "y1": 106,
                "x2": 827,
                "y2": 322
            },
            {
                "x1": 37,
                "y1": 128,
                "x2": 298,
                "y2": 389
            }
        ]
    },

    "time": 0.303
}

processing time is: 0.3306546211242676
```

test-script/test-demo/post_local_facial_landmark_py.py (default input file if without input parameter: face-detection-adas-0001.png)

Usage:

```
cd /opt/intel/service_runtime/test-script/
python3 ./test-demo/post_local_facial_landmark_py.py -i "IMAGE_FILE"
```

Result:

```json
{
    "ret": 0,
    "msg": "ok",
    "data": {
        "image_width": 916,
        "image_height": 502,
        "face_shape_list": [
            {
                "x": 684.5769672393799,
                "y": 198.69771003723145
            },
            {
                "x": 664.4035325050354,
                "y": 195.72683095932007
            },
            {
            "x": 243.1659236550331,
            "y": 211.56765642762184
            }
    ]
},
"time": 0.644
}
processing time is: 0.6765329837799072
```

test-script/test-demo/post_local_ocr_py.py (default input file if without input parameter: intel.jpg )
Usage:


```
cd /opt/intel/service_runtime/test-script/
python3 ./test-demo/post_local_ocr_py.py -i "IMAGE_FILE"
```

Result:

```
json
{
    "ret": 0,
    "msg": "ok",
    "data": {
        "item_list": [
            {
                "item": "",
                "itemcoord": [
                    {
                        "x": 159,
                        "y": 91,
                        "width": 144,
                        "height": 83
                    }

                ],
                "itemstring": "intel",
                "word": [
                    {
                        "character": "i",
                        "confidence": 0.9997048449817794
                    },
                    {
                        "character": "n",
                        "confidence": 7342.882333942596
                    },
                    {
                        "character": "t",
                        "confidence": 0.03543140404695105
                    },
                    {
                        "character": "e",
                        "confidence": 2068.173618863451
                    },
                    {
                        "character": "l",
                        "confidence": 0.006070811107476452
                    }
                ]
            },
            {
                "item": "",
                "itemcoord": [
                    {
                        "x": 203,
                        "y": 152,
                        "width": 176,
                        "height": 78
                    }
                ],
                "itemstring": "inside",
                "word": [
                    {
                        "character": "i",
                        "confidence": 0.9999999989186683
                    },
                    {
                        "character": "n",
                        "confidence": 4.0572939432830824e-08
                    },
                    {
                        "character": "s",
                        "confidence": 0.0015244375426548887
                    },
                    {
                        "character": "i",
                        "confidence": 3807.4854890027605
                    },
                    {
                        "character": "d",
                        "confidence": 0.42974367345764747
                    },
                    {
                        "character": "e",
                        "confidence": 0.008770792351958176
                    }
                ]
            }
        ]
    },
    "time": 5.69
}
processing time is: 5.702326774597168
```

test-script/test-demo/post_local_policy_py.py

**The default accelerator will be CPU if no change, once you set another accelerator with policy setting API, it will always take effect before you explicitly change it.**

Usage:

```
cd /opt/intel/service_runtime/test-script/
python3 ./test-demo/post_local_policy_py.py -d CPU -l 1
```

Result:

```
successfully set the policy daemon
processing time is: 0.004211902618408203
```

test-script/test-demo/post_local_tts_py.py (default input file if without input
parameter: test_sentence.txt)

**(So far, for easy testing of the pipeline, we had some rules on TTS input: the
input must be an English string and be saved in test_sentence.txt.)**

Usage:

```
cd /opt/intel/service_runtime/test-script/
sudo python3 ./test-demo/post_local_tts_py.py -s "SENTENCE_FILE"
```

Result:

```json
{
    "ret": 0,
    "msg": "ok",
    "data": {
        "format": 2,
        "speech": "UklGRjL4Aw..."
        "md5sum": "3bae7bf99ad32bc2880ef1938ba19590"
    },
    "time": 7.283
}
processing time is: 7.824979066848755
```



### 12.2.3 For testing C++ implementation of related REST APIs {#12.2.3}

test-script/test-demo/post_local_asr_c.py (the default input audio file("-a"):
how_are_you_doing.wav; the default inference device("-d"): GNA_AUTO)

usage:

```
cd /opt/intel/service_runtime/test-script/
python3 ./test-demo/post_local_asr_c.py -a "AUDIO_FILE" -d "DEVICE"
```

Result:

```
json
{
    "ret":0,
    "msg":"ok",
    "data":{
        "text":HOW ARE YOU DOING
    },
    "time":0.695
}
processing time is: 0.6860318183898926
```

test-script/test-demo/post_local_classfication_c.py (default input file if
without input parameter: classfication.jpg)

Usage:

```
cd /opt/intel/service_runtime/test-script/
python3 ./test-demo/post_local_classfication_c.py -i "IMAGE_FILE"
```

Result:

```
json
{
    "ret":0,
    "msg":"ok",
    "data":{
        "tag_list":[
            {"tag_name":'sandal',"tag_confidence":0.786503}

        ]
    },
    "time":0.380
}
processing time is: 0.36538004875183105
```

test-script/test-demo/post_local_face_detection_c.py (default input file if
without input parameter: face-detection-adas-0001.xml)

Usage:

```
cd /opt/intel/service_runtime/test-script/
python3 ./test-demo/post_local_face_detection_c.py -i "IMAGE_FILE"
```

Result:

```json
{
    "ret":0,
    "msg":"ok",
    "data":{
        "face_list":[
            {
                "x1":655,
                "y1":124,
                "x2":783,
                "y2":304
            },
            {
                "x1":68,
                "y1":149,
                "x2":267,
                "y2":367
            } ]
    },
    "time":0.305
}
processing time is: 0.3104386329650879
```

test-script/test-demo/post_local_facial_landmark_c.py (default input file if
without input parameter: face-detection-adas-0001.xml)

Usage:

```
cd /opt/intel/service_runtime/test-script/
python3 ./test-demo/post_local_facial_landmark_c.py -i "IMAGE_FILE"
```

Result:

```
json
{
    "ret":0,
    "msg":"ok",
    "data":{
        "image_width":916.000000,
        "image_height":502.000000,
        "face_shape_list":[
            {"x":684.691284,
             "y":198.765793},
            {"x":664.316528,
             "y":195.681824},
            {"x":241.314194,
             "y":211.847031}
           ]
    }
},
"time":0.623
}
processing time is: 0.6292879581451416
```

test-script/test-demo/post_local_ocr_c.py (default input file if without input
parameter: intel.jpg)

Usage:

```
cd /opt/intel/service_runtime/test-script/
python3 ./test-demo/post_local_ocr_c.py -i "IMAGE_FILE"
```

Result:
```json
{
    "ret":0,
    "msg":"ok",
    "data":{
        "item_list":[
            {
                "itemcoord":[
                    {
                        "x":161.903748,
                        "y":91.755684,
                        "width":141.737503,
                        "height":81.645004
                    }
                ],
                "words":[
                    {
                        "character":i,
                        "confidence":0.999999
                    },
                    {
                        "character":n,
                        "confidence":0.999998
                    },
                    {
                        "character":t,
                        "confidence":0.621934
                    },
                    {
                        "character":e,
                        "confidence":0.999999
                    },
                    {
                        "character":l,
                        "confidence":0.999995
                    } ],
                "itemstring":intel
            },
            {
                "itemcoord":[
                    {
                        "x":205.378326,
                        "y":153.429291,
                        "width":175.314835,
                        "height":77.421722
                    }
                ],
                "words":[
                    {
                        "character":i,
                        "confidence":1.000000
                    },
                    {
                        "character":n,
                        "confidence":1.000000
                    },
                    {
                        "character":s,
                        "confidence":1.000000
                    },
                    {
                        "character":i,
                        "confidence":0.776524
                    },
                    {
                        "character":d,
                        "confidence":1.000000
                    },
                    {
                        "character":e,
                        "confidence":1.000000
                    } ],
                "itemstring":inside
            } ]
    },
    "time":1.986
}
processing time is: 1.975726842880249
```

test-script/test-demo/post_local_speech_c.py (default input file if without
input parameter: dev93_1_8.ark)

**This case is used to verify GNA accelerators. The default setting is GNA_AUTO.
If GNA HW is ready, the inference will run on GNA HW. Otherwise, it will run on
GNA_SW mode to simulate GNA HW.**

usage:

```
cd /opt/intel/service_runtime/test-script/
python3 ./test-demo/post_local_speech_c.py
```

Result:

```
json
{
    "ret":0,
    "msg":"ok",
    "data":{
        "input information(name:dimension)":{
            "Parameter":[8,440]
        },
        "output information(name:dimension)":{
            "affinetransform14/Fused_Add_":[8,3425]
        }
    },
    "time":0.344222
}
{
    "ret":0,
    "msg":"ok",
    "data":{
        "result":"success!"
    },
    "time":0.484783
}
fcgi inference time: 0.009104
processing time is: 0.0262906551361084
```

test-script/test-demo/post_local_policy_c.py

**The default accelerator will be CPU if no change, once you set another
accelerator with policy setting API, it will always take effect before you
explicitly change it.**

Usage:

```
cd /opt/intel/service_runtime/test-script/
python3 ./test-demo/post_local_policy_c.py -d CPU -l 1
```

Result:

```
successfully set the policy daemon
processing time is: 0.0035839080810546875
```



### 12.2.4 For testing C++ implementation of related gRPC APIs {#12.2.4}

grpc_inference_service_test.py

Usage:

```
cd /opt/intel/service_runtime/test-script/
python3 ./test-grpc/grpc_inference_service_test.py
```

Result:

```
HealthCheck resp: 0
ASR result:
{"text":"HOW ARE YOU DOING"}
Classification result:
[{"tag_name":"sandal","tag_confidence":0.743236}]
FaceDetection result:
{"x1":611,"y1":106,"x2":827,"y2":322},{"x1":37,"y1":128,"x2":298,"y2":389}
FacialLandmark result:
{"x":684,"y":198},{"x":664,"y":195}, …
OCR result:
[{"itemcoord":{"x":162,"y":91,"width": …
```



## 12.3 Health-monitor mechanism test case {#12.3}

### 12.3.1 Test case {#12.3.1}

This test case will take about two minutes, please wait.
test-script/test-health-monitor/test_health_monitor.sh
Usage:

```
cd /opt/intel/service_runtime/test-script/
./test-health-monitor/test_health_monitor.sh
```

Result:

```
*******************************
test fcgi and grpc daemon:
fcgi can automatic restart
grpc can automatic restar
*******************************
test container:
f64fed060cf1 fe50c5747d46 "/start.sh" 2 minutes ago Up 2 minutes (unhealthy)
0.0.0.0:8080-8081->8080-8081/tcp service_runtime_container
service_runtime_container
restart container...
7f20255d36a9 fe50c5747d46 "/start.sh" About a minute ago Up About a minute
(health: starting) 0.0.0.0:8080-8081->8080-8081/tcp service_runtime_container
container can automatic restart
```


### 12.3.2 How it works (in brief) {#12.3.2}

The health monitor mechanism consisted of 2 parts: health-monitor daemon
installed in the host system, and its agent installed inside the container.

The agent will check all background running services, daemons and API gateways
in a 60 seconds (it is the default value, can be customized via parameter to
start command) interval and report the healthy status to the host
health-monitor. Meanwhile, under situations where daemon or services fail to
respond, according to specific cases, the agent may try to restart those failed
processes and then confirm those processes work normally, or rely on API
gateways to restart related services and then confirm those services work
normally. Whichever cases, the agent will report those information to
health-monitor as record and the preconditions of taking additional actions if
needed. If the agent itself or API gateways cannot make the processes work
again, then that information will also be reported to the host health-monitor
and then the health-monitor will decide how to restart the docker instance
according to some predefined rules.

In the test case above, it will try to kill these services and container
instance respectively and then re-check the status to make sure the health
monitor mechanism works as expected. The output log 'xxxxxx restart' is meaning
related services/components were killed and then restarted successfully.

For health monitor related log, you can find them by:

```
    $> sudo journalctl -f -u service-runtime-health-monitor.service
```

## 12.4 Deb package for host installed application/service (if not install yet) {#12.4}

**Note: If not for testing the OTA process, then please uninstall existing
packages before installing the new ones to avoid "possible" conflicts with OTA
logic.**

```
service-runtime_1.0-210201_all.deb
service-runtime-test_1.0-210201_all.deb
```

Installation instructions:

```
dpkg -i *.deb
```



## 12.5 Deb package for host installed neural network models (if not install yet) {#12.5}

**Note: If not for testing the OTA process, then please uninstall existing
packages before install the new ones to avoid "possible" conflicts with OTA
logic.**

```
service-runtime-models-classification_1.0-210201_all.deb
service-runtime-models-deeplab_1.0-210201_all.deb
service-runtime-models-face-detection_1.0-210201_all.deb
service-runtime-models-facial-landmarks_1.0-210201_all.deb
service-runtime-models-lspeech-s5-ext_1.0-210201_all.deb
service-runtime-models-ocr_1.0-210201_all.deb
service-runtime-models-tts_1.0-210201_all.deb
service-runtime-models-wsj-dnn5b_1.0-210201_all.deb
service-runtime-models-cl-cache_1.0-210201_all.deb
```

Installation instructions:

```
dpkg -i *.deb
```
