#!/bin/bash ../.port_include.sh
port=libevent
version=2.1.12-stable
files="https://github.com/libevent/libevent/releases/download/release-${version}/libevent-${version}.tar.gz libevent-${version}.tar.gz
https://github.com/libevent/libevent/releases/download/release-${version}/libevent-${version}.tar.gz.asc libevent-${version}.tar.gz.asc"

useconfigure="true"
depends="openssl"

auth_type="sig"
auth_import_key="9E3AC83A27974B84D1B3401DB86086848EF8686D"
auth_opts="libevent-${version}.tar.gz.asc"

export PKG_CONFIG_PATH=${SERENITY_ROOT}/Build/Root/usr/local/lib/pkgconfig
