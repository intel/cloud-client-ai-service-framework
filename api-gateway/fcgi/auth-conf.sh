#!/bin/bash

if [[ -r "${AIAPI_SERVER_KEY:=/etc/lighttpd/server.key}" &&
      -r "${AIAPI_SERVER_CERT:=/etc/lighttpd/server.pem}" ]]; then
    cat <<- EOF
	server.modules          += ( "mod_openssl" )
	ssl.engine               = "enable"
	ssl.pemfile              = "$AIAPI_SERVER_CERT"
	ssl.privkey              = "$AIAPI_SERVER_KEY"
	ssl.openssl.ssl-conf-cmd = ( "MinProtocol" => "TLSv1.2" )
	EOF
fi

dbpath() {
    sed -n 's/^auth .* pam_userdb\.so .* db=\([^ ]*\).*$/\1.db/p' $1
}
if [[ -r "/etc/pam.d/${AIAPI_PAM_SERVICE:=http}" &&
      -r "$(dbpath /etc/pam.d/$AIAPI_PAM_SERVICE)" ]]; then
    cat <<- EOF
	server.modules          += ( "mod_authn_pam", "mod_auth" )
	auth.backend.pam.opts    = ( "service" => "$AIAPI_PAM_SERVICE" )
	auth.backend             = "pam"
	auth.require             = ( "/cgi-bin/" => (
	                               "method"  => "basic",
	                               "realm"   => "AIAPI Gateway",
	                               "require" => "valid-user"
	                           ) )
	EOF
fi
