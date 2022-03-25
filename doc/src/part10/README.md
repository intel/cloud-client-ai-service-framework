# 10. How to enable DNS interception

DNS interception is useful to redirect external AI service invocation to the local CCAI container. Suppose that api.aiservice.com is a public AI service provider. When applications try to connect to it, the server's IP address must be obtained before through DNS. With the DNS interception enabled, the interceptor will return the container's IP address to the applications instead of the real address of the server. So all connections to api.service.com will be redirected to the local CCAI container.

Our implementation is an Name Service Switch (NSS) module for the Linux system instead of a standalone daemon process to hijack DNS traffic. So the
installation and enabling process follow the NSS configuration. Please see the manual page of nsswitch.conf(5) for detailed information.

1. Copy the dynamic library to the system library directory:

        $> sudo cp -fp libnss_ccai.so.2 /lib/x86_64-linux-gnu/

2. Modify the /etc/nsswitch.conf

        $> sudo sed -i '/\^hosts:/{s/ ccai / /g;s/ dns/ ccai dns/g}' /etc/nsswitch.conf

Or manually edit the file to insert the "ccai" module before the "dns" module in the "hosts" line. The line should look like this:

    hosts: files mdns4_minimal [NOTFOUND=return] ccai dns

3. Create a configuration file for mapping name to container IP address

        $> sudo sh -c "cat > /etc/host_ccai"
        # this is an example
        172.18.0.2 api.aiservice.com
        ^D

The format of this file is the same as the traditional /etc/hosts file.

To exam the configuration, run the getent command:

    $> getent ahosts api.aiservice.com
    
    172.18.0.2 STREAM api.aiservice.com
    
    172.18.0.2 DGRAM
    
    172.18.0.2 RAW
    
    ffff::ac12:2 STREAM
    
    ffff::ac12:2 DGRAM
    
    ffff::ac12:2 RAW