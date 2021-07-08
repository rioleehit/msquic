#!/bin/bash
set -e
set -x


# Set directory
SCRIPTPATH=`pwd`
# SCRIPTPATH=`dirname $0`
echo $SCRIPTPATH
OPENSSL_DIR=$SCRIPTPATH/submodules/openssl
OPENSSL_PREFIX=$SCRIPTPATH/build/
export ANDROID_NDK_HOME=$SCRIPTPATH/android-ndk-r21e
export ANDROID_NDK_ROOT=$SCRIPTPATH/android-ndk-r21e

# Find the toolchain for your build machine
toolchains_path=$(python3 toolchains_path.py --ndk ${ANDROID_NDK_HOME})

# Configure the OpenSSL environment, refer to NOTES.ANDROID in OPENSSL_DIR
# Set compiler clang, instead of gcc by default
CC=clang

# Add toolchains bin directory to PATH
PATH=$toolchains_path/bin:$PATH

# Set the Android API levels
ANDROID_API=21

# Set the target architecture
# Can be android-arm, android-arm64, android-x86, android-x86 etc
architecture=android-arm

# Create the make file
cd ${OPENSSL_DIR}
./Configure ${architecture} -D__ANDROID_API__=$ANDROID_API --prefix=$OPENSSL_PREFIX
# ./Configure ${architecture} -D__ANDROID_API__=$ANDROID_API -DOPENSSL_NO_STDIO enable-tls1_3 no-makedepend no-dgram no-ssl3 no-psk no-srp no-autoerrinit no-filenames no-ui-console no-err no-zlib no-egd no-idea no-rc5 no-rc4 no-afalgeng no-comp no-cms no-ct no-srp no-srtp no-ts no-gost no-dso no-ec2m no-tls1 no-tls1_1 no-tls1_2 no-dtls no-dtls1 no-dtls1_2 no-ssl no-ssl3-method no-tls1-method no-tls1_1-method no-tls1_2-method no-dtls1-method no-dtls1_2-method no-siphash no-whirlpool no-aria no-bf no-blake2 no-sm2 no-sm3 no-sm4 no-camellia no-cast no-des no-md4 no-mdc2 no-ocb no-rc2 no-rmd160 no-scrypt no-weak-ssl-ciphers no-shared no-tests -DL_ENDIAN 
# ./Configure ${architecture} -D__ANDROID_API__=$ANDROID_API -DOPENSSL_NO_STDIO enable-tls1_3 no-makedepend no-dgram no-ssl3 no-psk no-srp no-autoerrinit no-filenames no-ui-console no-err no-zlib no-egd no-uplink no-idea no-rc5 no-rc4 no-afalgeng no-acvp_tests no-comp no-cmp no-cms no-ct no-srp no-srtp no-ts no-fips no-gost no-padlockeng no-dso no-ec2m no-tls1 no-tls1_1 no-tls1_2 no-dtls no-dtls1 no-dtls1_2 no-ssl no-ssl3-method no-tls1-method no-tls1_1-method no-tls1_2-method no-dtls1-method no-dtls1_2-method no-siv no-siphash no-whirlpool no-aria no-bf no-blake2 no-sm2 no-sm3 no-sm4 no-camellia no-cast no-des no-md4 no-mdc2 no-ocb no-rc2 no-rmd160 no-scrypt no-weak-ssl-ciphers no-shared no-tests -DL_ENDIAN 
# --prefix=$OPENSSL_PREFIX
# -DOPENSSL_NO_STDIO enable-tls1_3 no-makedepend no-dgram no-ssl3 no-psk no-srp no-autoerrinit no-filenames no-ui-console no-err no-zlib no-egd no-idea no-rc5 no-rc4 no-afalgeng no-comp no-cms no-ct no-srp no-srtp no-ts no-gost no-dso no-ec2m no-tls1 no-tls1_1 no-tls1_2 no-dtls no-dtls1 no-dtls1_2 no-ssl no-ssl3-method no-tls1-method no-tls1_1-method no-tls1_2-method no-dtls1-method no-dtls1_2-method no-siphash no-whirlpool no-aria no-bf no-blake2 no-sm2 no-sm3 no-sm4 no-camellia no-cast no-des no-md4 no-mdc2 no-ocb no-rc2 no-rmd160 no-scrypt no-weak-ssl-ciphers no-shared no-tests -DL_ENDIAN 
# Build
make

# Copy the outputs
OUTPUT_INCLUDE=$SCRIPTPATH/output/include
OUTPUT_LIB=$SCRIPTPATH/output/lib/${architecture}
mkdir -p $OUTPUT_INCLUDE
mkdir -p $OUTPUT_LIB
cp -RL include/openssl $OUTPUT_INCLUDE
cp libcrypto.so $OUTPUT_LIB
cp libcrypto.a $OUTPUT_LIB
cp libssl.so $OUTPUT_LIB
cp libssl.a $OUTPUT_LIB