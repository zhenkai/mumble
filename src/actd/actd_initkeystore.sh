#!/bin/sh
# lib/ccn_initkeystore.sh
# 
# Part of the CCNx distribution.
#
# Copyright (C) 2009-2010 Palo Alto Research Center, Inc.
#
# This work is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License version 2 as published by the
# Free Software Foundation.
# This work is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.
#
# Create a ccn keystore without relying on java
: ${RSA_KEYSIZE:=1024}
: ${CCN_USER:=`id -n -u`}
Fail () {
  echo '*** Failed' "$*"
  exit 1
}

cd $HOME
test -d .actd && rm -rf .actd
test $RSA_KEYSIZE -ge 512 || Fail \$RSA_KEYSIZE too small to sign CCN content
(umask 077 && mkdir .actd) || Fail $0 Unable to create .ccnx directory
cd .actd
umask 077
# Set a trap to cleanup on the way out
trap 'rm -f openssl.cnf' 0
cat <<EOF >openssl.cnf
# This is not really relevant because we're not sending cert requests anywhere,
# but openssl req can refuse to go on if it has no config file.
[ req ]
distinguished_name	= req_distinguished_name
[ req_distinguished_name ]
countryName			= Country Name (2 letter code)
countryName_default		= AU
countryName_min			= 2
countryName_max			= 2
EOF
openssl req    -config openssl.cnf      \
               -newkey rsa:$RSA_KEYSIZE \
               -x509                    \
               -keyout actd_private_key.pem  \
               -out actd_cert.pem         \
               -subj /CN="$CCN_USER"    \
               -nodes                                   || Fail openssl req

openssl pkcs12 -export -name "ccnxuser" \
               -out .actd_keystore      \
               -in actd_cert.pem          \
               -inkey actd_private_key.pem   \
               -password pass:'Th1s1s@p8ssw0rdf0r8ctd.' || Fail openssl pkcs12
