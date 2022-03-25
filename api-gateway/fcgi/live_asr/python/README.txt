# Live ASR(Automatic Speech Recognition) Python Sample

This sample demonstrates how to execute an ASR case based on live ASR API provided by the api-gatway module.

This sample will continuously capture the voice from the MIC on the host PC, do inference, and
send back the sentences.

Before enabling this case, you need to enable the pulseaudio and health-monitor services.
  (1) On host PC, install the pulseaudio package if this package isn't been installed.
      For example: 
         #sudo apt-get install pulseaudio
  (2) Enable TCP protocol of the pulseaudio.
      Edit the configuration file. for example:
         #sudo vim /etc/pulse/default.pa
      Find out the following tcp configuration:
         #load-module module-native-protocol-tcp
      Uncomment the tcp configuration(remove "#"),and add authentication:
         load-module module-native-protocol-tcp auth-anonymous=1
      Save and quit the configuration file.
  (3) Restart the pulseaudio service. For example:
         #sudo systemctl restart pulseaudio 
      or kill the pulseaudio thread
         #sudo kill -9 xxxx(pulseaudio thread number)
  (4) Runing the health-monitor service on the host pc if you don't run it.
      This service is used to monitor the CCAI container.
