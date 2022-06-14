           Automatic Speech Recognition(ASR) Service

This ASR service works with a pretrained Mozilla* DeepSpeech 0.8.2 model or DeepSpeech 0.9.3 ZH model.
It recognizes both English and Mandarin Chinese. DeepSpeech 0.8.2 model is trained with English, while
DeepSpeeach 0.9.3 ZH model is trained with Mandarin Chinese.

This service has two modes:
  - Offline mode
    The client sets params['realtime'] = 'OFFLINE'.
    In this mode, the client should read an audio file in PCM WAV 16 kHz mono format.
    And send audio file parameters, such as sample-width, sampling rate, and audio data to the ASR
    servcie. Once the audio service receives these data, it will loads acoustic model and language model
    to recoginze the audio data. Then the service sends the reconized result back to the client.
 -  Online mode
    The client sets params['realtime'] = 'ONLINE_READ'.
    In this mode, the ASR service will read audio data from the MIC directly. This is a real time speech
    recognization. The MIC data is fed to the VAD module at real-time speed. The client need to sent post
    request('ONLINE_READ') continuously to the ASR service to get the recognized result on time. For 
    example: the client sends a post request evry second.
    In this mode, the MIC data is fed to the VAD module to be detected whether it is a valid speech. The
    valid speech data is then fed to the acoustic model and language model.
    If the client wants to stop online mode, it should sets params['realtime'] = 'ONLINE_STOP'. Send a stop
    post request to ASR service to stop online mode.
 -  Notice
    The client needs to set language parameter to specify which language the ASR service should deal with.
    params['language'] = 'ENGLISH' for English language or  params['language'] = 'CHINESE' for chinese language.

An example code for client side, please refer to post_local_asr_py.py 

Supported Models
   English: mozilla-deepspeech-0.8.2-models
   Chines:  mozilla-deepspeech-0.9.3-models-zh-CN

   Please pay attention to the model license, Mozilla Public License 2.0.
   
   This ASR service uses the neural network in Intermediate Representation (IR) format, sopported by Openvino.
   The details on how to convert mozilla models to OpenVINO Inference Engine format (*.xml + *.bin), please 
   refer to https://github.com/openvinotoolkit/open_model_zoo/tree/2021.4/demos/speech_recognition_deepspeech_demo/python

   For Mandarin model (IR format), please contact the author to get it.

The ASR part is based on https://github.com/openvinotoolkit/open_model_zoo/tree/2021.4/demos/speech_recognition_deepspeech_demo,
adding the feature of recognizing Mandarin Chinese language.

In "Online" mode, in order to use the MIC in the CCAI container, you need to enable the pulseaudio and health-monitor services on
the host side:
  (1) On host PC, install the pulseaudio package if this package isn't been installed.
      For example:
         #sudo apt-get install pulseaudio
  (2) Enable TCP protocol of the pulseaudio.
      Edit the configuration file. for example:
         #sudo vim /etc/pulse/default.pa
      Find out the following tcp configuration:
         #load-module module-native-protocol-tcp
      Uncomment the tcp configuration(remove "#"), and add authentication:
         load-module module-native-protocol-tcp auth-anonymous=1
      Save and quit the configuration file.
  (3) Restart the pulseaudio service. For example:
      Stop the pulseaudio:
         # pulseaudio -k
      Start the pulseaudio:
         #pulseaudio --start or #pulseaudio -D
  (4) Runing the health-monitor service on the host pc if you don't run it.
      This service is used to monitor the CCAI container.

