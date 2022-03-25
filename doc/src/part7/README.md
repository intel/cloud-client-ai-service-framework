# 7. How to use AI services provided by CCAI

As mentioned above in chapter 6, CCAI services work mode are:

![](../media/02849905817820bd1ccb2f929176f385.png)

AI services for CCAI include two parts, one is client-side, the other is
server-side. Customer applications are so called client-side. The CCAI services are server-side. Client-side sends http post requests or gRPC requests to server-side, and server-side replies responses to client-side.

## 7.1 Request serving via REST APIs {#7.1}

For using REST APIs providing by CCAI, the common steps for implementing a
request in your client application are:

1) Construct post request

    -- url: AI services address. for example: url=
    '<http://localhost:8080/cgi-bin/fcgi_py_tts>>'.If client-side and server-side are not on the same machine, the localhost needs to be replaced with your ip address.  
    -- post_parameter: different for different AI services.

2) sending post request to fcgi AI service:
   *response = requests.post(url, post_parameter)*

3) Get the inference result from AI services. *The response is the result.*

Please refer to 10.1( FCGI APIs Manual) for detailed steps to implement
different AI client applications.

## 7.2 Request serving via gRPC APIs {#7.2}

For using gRPC APIs providing by CCAI, the common steps for implementing a
request in your client application are:

1) create call credential

    *metadata_plugin = BasicAuthenticationPlugin(username, password)*

    *call_cred = grpc.metadata_call_credentials(metadata_plugin)*

2) Create channel credential

    *channel_cred = grpc.ssl_channel_credentials()*

3) Open grpc secure channel, If client-side and server-side are not on the same machine, the localhost needs to be replaced with your ip address.

    *credentials = grpc.composite_channel_credentials(channel_cred, call_cred) with grpc.secure_channel('localhost:' + port, credentials) as channel:*

4) Get the result

    *stub = inference_service_pb2_grpc.InferenceStub(channel)*

## 7.3 Proxy setting {#7.3}

If you are behind a firewall or developing within another container or VM and want to communicate with a CCAI container running in the same physical machine, and your system has a proxy setting, you may need to check the proxy setting, the service IP address be set into the no_proxy.

