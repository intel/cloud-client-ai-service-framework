
           How to convert TTS module to Openvino IR format

The following are steps on how to convert the TTS model to the Intermediate Representation (IR) which
is uesed in CCAI TTS case. Files under the folder("support") will be used in these steps. 

For the detail usage on how to use the Model Optimizer, please refer to https://github.com/openvinotoolkit/openvino

STEPS:
1: Donwload tacotron code from the github.
        #git clone https://github.com/Kyubyong/tacotron.git
2: Download pretrain model.
        [LJ 200k](https://www.dropbox.com/s/8kxa3xh2vfna3s9/LJ_logdir.zip?dl=0)
3: Enter the tacotron codebase downloaded.
        #cd tacotron
4: Create the folder named "models"
        #mkdir models
        #mkdir models/pb	 
   Copy model files(downloaded in step2) to this folder(./models).
        #cp [LJ 200k] ./models/
5: pip3 install the following models:
        tensorflow(1.5.0)   tensorboard(1.6.0)  pqdm matplotlib
6: Copy "./support/generate_frozen_model.py" and "./support/train.py" to the tactron codebase.
        # cp ./support/generate_frozen_model.py ./tacotron/
        # cp ./support/train.py ./tactron/
    Replace the original file: train.py 
7: Generate frozen models.
        # python3 generate_frozen_model.py
   If everything is ok, two frozen models are generated under ./models/pb
   The two frozen models are: infer_ie_decode.pb  and infer_ie_post.pb	
8: Do some modification for openvino Model Optimizer files.
   Use the following files to replace original files under Model Optimizer folder.
        #sudo cp -f  ./support/TensorIteratorCondition.py  /opt/intel/openvino/deployment_tools/model_optimizer/extension/middle/TensorIteratorCondition.py
	#sudo cp -f ./support/fake_const_ext.py  /opt/intel/openvino/deployment_tools/model_optimizer/extensions/front/tf/fake_const_ext.py
9: Using the Model Optimizer tool to convert frozen models to openvino IR format.
   Copy ./support/convert_ie.sh file to ./models/pb/ folder
        #cp -f ./support/convert_ie.sh ./models/pb/
   Running  convert_ie.sh to generate Openvino IR format files
        #./convert_ie.sh
   If evrything is ok, IR format files are generated. They are:
        infer_ie_decode.bin   infer_ie_decode.xml  infer_ie_post.bin  infer_ie_post.xml
10: Rename files
        #mv infer_ie_decode.bin tts-encoder-decoder.bin
        #mv infer_ie_decode.xml tts-encoder-decoder.xml
        #mv infer_ie_post.bin tts-postprocessing.bin
        #mv infer_ie_post.xml tts-postprocessing.xml	  
