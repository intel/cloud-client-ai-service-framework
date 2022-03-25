
#the first graph (encode+decode)
python3 /opt/intel/openvino/deployment_tools/model_optimizer/mo_tf.py --input_model infer_ie_decode.pb \
--input x,y --input_shape [1,43],[1,200,400] \
--output decode_out

#the second graph (post process)
python3 /opt/intel/openvino/deployment_tools/model_optimizer/mo_tf.py --input_model infer_ie_post.pb \
--input y --input_shape [1,200,400] \
--output output


