# TTS(TEXT-TO_SPEECH) Python Sample

This sample demonstrates how to execute an text-to-speech case based on TTS API provided by the api-gatway module.

There are two path for output data of this case:
  - the first path: the wave data generated are written to a wave file.
  - the second path: the wave data generated are sent to the speakers directly, so you can hear the sentence.

For the second path, you need to enable the pulseaudio and health-monitor services:  
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
         #sudo systemctl restart pulseaudio 
      or kill the pulseaudio thread
         #sudo kill -9 xxxxx(pulseaudio thread number)
  (4) Runing the health-monitor service on the host pc if you don't run it.
      This service is used to monitor the CCAI container.
