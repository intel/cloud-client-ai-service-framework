# 9. How to enable Encryption and Authentication for CCAI

The framework supports encryption and authentication. You can choose to enable
both or any one of them. For security reasons, it is better to have both of them
enabled. But these features are disabled by default for better performance,
because the server and clients are both running on localhost. Encryption and
authentication can be enabled by changing the configuration files.

## 9.1 Encryption {#9.1}

When encryption is enabled, the TLS is applied to communication between server and clients. We highly recommend that TLS v1.2 or v1.3 is used for encryption. TLS v1.0 and v1.1 are obsolete now. It should be only used for compatibility with old clients.

To enable encryption, a server private key and a certificate is required. The certificate can be acquired from a CA (Let's encrypt for example) or self signed as below showing.

1)Generate server key and certificate (for self-signed certificate only):

    $> cat > certificate.conf << EOF
    [req]
    default_bits = 4096
    prompt = no
    default_md = sha256
    req_extensions = req_ext
    distinguished_name = dn
    [dn]
    C = CN
    ST = BJ
    O = IAGS SSE AEE Team
    CN = localhost
    [req_ext]
    subjectAltName = @alt_names
    [alt_names]
    DNS.1 = localhost
    IP.1 = ::1
    IP.2 = 127.0.0.1
    EOF
    
    $> openssl genrsa -out ca.key 4096
    
    $> openssl req -new -x509 -key ca.key -sha256 -subj "/C=CN/ST=BJ/O=Fake CA" -days 365 -out ca.crt
    
    $> openssl genrsa -out server.key 4096
    
    $> openssl req -new -key server.key -out server.csr -config certificate.conf
    
    $> openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.pem -days 365 -sha256 -extfile certificate.conf -extensions req_ext

There are several files generated. server.key is the server private key, and server.pem is the server certificate. ca.crt is the local root CA certificate which clients need to trust.

2)Enable TLS

To make the key and certificate file accessible to REST and gRPC server in container, start docker container with arguments:

    -v /path/to/private_key:/etc/lighttpd/server.key:ro -v /path/to/certificate:/etc/lighttpd/server.pem:ro

/path/to/private_key and /path/to/certificate are the paths in the host where you save the key and certificate files generated in step 1) above.

This can be done by adding the line above as parameters to "docker run" command in docker launching script located in:

    /opt/intel/service_runtime/service_runtime.sh

Pay attention to the permission of the database in the container. It must be readable to lighttpd and nghttpx daemons.

**Note: once enabled encryption, all REST APIs in all REST API based test cases above or in your own application should change those URLs from 'http' to 'https' for valid access.**

## 9.2 Enable authentication {#9.2}

1)Create user account database

    #install Berkeley DB command line tools
    
    $> sudo apt install db5.3-util
    
    # add or change a user into the database
    
    $ { echo "username"; openssl passwd -6 "password"; } | db5.3_load -Tt hash apiuser.db
    
    # to examine the database
    
    $> db5.3_dump -p apiuser.db

These are the basic operations to manipulate the account database. You can write a script to make life easier. Here is an example script from the official linux-pam repo:
<https://raw.githubusercontent.com/linux-pam/linux-pam/master/modules/pam_userdb/create.pl>>.

2)Enable authentication

To make the account database accessible to REST and gRPC server in container, start docker container with arguments:

    -v /path/to/database:/etc/lighttpd/apiuser.db:ro

/path/to/database is the path in the host where you save the database generated
in step 1) above.

This can be done by adding the line above as parameters to "docker run" command in docker launching script located in:

    /opt/intel/service_runtime/service_runtime.sh

Pay attention to the permission of the database in the container. It must be readable to lighttpd and grpc_inference_service daemons.
