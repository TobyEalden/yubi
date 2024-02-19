# introduction

A set of scripts and tools to test the use of a YubiKey for client authentication in a mutually authenticated HTTPS connection.

# initial investigation and setup

There is an npm package `pkcs11js` that can be used to access the YubiKey's PKCS#11 interface, but there is no way of integrating it with the nodejs TLS modules.

As a consequence, the client request is sent via a tool built with a C++ application that uses the PKCS#11 interface to access the YubiKey and integrates it with the OpenSSL library to initialise an SSL context and send requests to the server.

To set up the proof of concept, the following steps were taken:

1. Generate a key pair on the YubiKey using the `ykman` command line tool (see below). The public key is exported to a file called `public.pem`.
2. Generate a CSR using the `ykman` command line tool (see below). The CSR is written to a file called `csr.pem`.
3. Set up a simple nodejs server that automatically creates a self-signed CA and uses it to issue a server certificate. This server then creates an HTTPS listener and also exposes two endpoints: one to request a certificate and one to test the mutually authenticated flow.
4. Send the CSR to the server `/csr` endpoint and receive the signed certificate using the `send-csr.js` script. This was done via a nodejs script for simplicity as this request is not part of the mutually authenticated flow. The response from the server is a certificate that is written to a local file called `client-cert.pem`.
5. Import the newly acquired client certificate into the Yubikey. This is necessary because the way PIV works is that you can only see certificates in a slot. The presence of a certificate assumes that the corresponding private key is also present.
6. Copy the `ca.pem` and `client-cert.pem` files to the `yubi-client` directory. The `ca.pem` file is the server's CA file and the `client-cert.pem` file is the client certificate.
7. The C++ client `yubi-client` is then used to establish a mutually authenticated connection to the server and send a JSON request to the `/test` endpoint. The client uses the `client-cert.pem` file as the client certificate and the server's CA file `ca.pem` to establish the connection.
8. The server receives the request, verifies the client certificate and then responds with a JSON response, which is then printed to the console by the client.

# ykman commands

Some useful commands for the `ykman` command line tool.

The default PIN is 123456.

I had problems using the Yubikey on macOS after changing the default PIN.

## generate key

```shell
ykman piv keys generate --algorithm RSA2048 9a public.pem
```

## generate CSR

This requires the public key to be available in a file called `public.pem`. You may also want to change the subject information in the `--subject` option.

```shell
ykman piv certificates request 9a public.pem csr.pem --subject "CN=Example,DC=example.com"
```

## import certificate

This requires the certificate to be available in a file called `client-cert.pem`, so you will need to have received the certificate from the server first by running the `node ./send-csr.js` script (see below).

```shell
ykman piv certificates import 9a client-cert.pem
```


# yubi-server

This is a simple nodejs server that can be used to test the mutually authenticated flow.

When first run, it will generate a self-signed CA and then use it to issue a server certificate that will be used to back the HTTP server.

Install: 

```
npm install
```

Run:

```shell
node ./index.js
```

The HTTP server exposes two endpoints:

## /csr

This endpoint is used to request a certificate from the server.

The client should POST a CSR to this endpoint as JSON request body. The server will then sign the CSR and return the certificate as JSON response body.

This endpoint is not authenticated, i.e. it can be accessed by any client.

## /test

This endpoint is used to test the mutually authenticated flow.

The client should send a JSON POST request of the form `{"operand": 42}`. The server will double the operand and then respond with a JSON response of the form `{"result": 84}`.

# send CSR

The `send-csr.js` is a simple nodejs client that can be used to send a CSR to the server and receive the signed certificate.

It requires a file named `csr.pem` (created using the `ykman` as described above), as well as the server's CA file `ca.pem`.

It simply sends the csr to the `/csr` endpoint and then saves the received certificate to a file named `client-cert.pem`.

To run, make sure the server is running and then run the following command:

```shell
node ./send-csr.js
```

# yubi-client

This is a test client that establishes a mutually authenticated HTTPS connection to the `yubi-server` and sends a JSON request to the `/test` endpoint.

It requires that the client certificate is available in a file named `client-cert.pem` and the server CA is available in a file named `ca.pem`.

It then creates an SSL connection to the server at https://127.0.0.1:4443 using the private key stored on the yubikey.

It mandates that the server is backed by a certificate issued by `ca.pem`.

You may need to set `OPENSSL_CONF` to point to the correct openssl configuration file, e.g. `/home/pi/code/yubi/openssl-release`.

# build notes

The demo has been built and tested on an Apple Silicon macOS and a Raspberry Pi 5 aarch64 running Debian.

Various libraries were built from source for the `yubi-client` application. The following notes may be useful if you need to build the application on a different system.

## openssl

The OpenSSL library was built from source, primarily because I was using 1.1.1 locally and want to try 3.x. Chances are that it wouldn't be necessary to build it from source in most cases as version 3.x tends to be the default on most systems these days.

Note the use of hard-coded paths below, you will need to adjust these to match your system.

```shell
git clone https://github.com/openssl/openssl
cd openssl
./Configure linux-armv4 no-shared --prefix=~/code/yubi/openssl-release --openssldir=~/code/yubi/openssl-release
make
make install_sw
```

## libp11

The libp11 library provides a pkcs11 engine plugin for the OpenSSL library allows accessing PKCS#11 modules in a semi-transparent way.

Note the use of hard-coded paths below, you will need to adjust these to match your system.

```shell
wget https://github.com/OpenSC/libp11/releases/download/libp11-0.4.12/libp11-0.4.12.tar.gz
tar -xvf libp11-0.4.12.tar.gz
cd libp11-0.4.12
export LDFLAGS=-L/Users/tobyealden/code/tdxvolt/openssl-release/lib
export PKG_CONFIG_PATH=/Users/tobyealden/code/tdxvolt/openssl-release/lib/pkgconfig
export PKG_CONFIG_LIBDIR=/Users/tobyealden/code/tdxvolt/openssl-release/lib/pkgconfig
export OPENSSL_LIBS="-L/Users/tobyealden/code/tdxvolt/openssl-release/lib -lcrypto"
export LIBS="-latomic"
./configure --prefix=/Users/tobyealden/code/tdxvolt/openssl-release
make
make install
```

## ykman CLI

The `ykman` tool is used for interacting with the Personal Identity Verification (PIV) application on a YubiKey.

The following commands were used to install the `ykman` tool macOS.

```shell
brew install ykman
``` 

The equivalent command for Debian is:

```shell
sudo apt-get install yubikey-manager
```

## opensc

OpenSC provides a set of libraries and utilities to work with smart cards.

The following commands were used to install the `opensc` tool on macOS.

```shell
brew install opensc
```

The equivalent command for Debian is:

```shell
sudo apt-get install opensc
```

You then need to modify the `openssl.cnf` file to configure the pcks11 engine, see https://github.com/OpenSC/libp11?tab=readme-ov-file#using-the-engine-from-the-command-line.

The relevant excerpts of an example configuration is shown below, you will need to adjust the paths to match your system. 

```
openssl_conf = openssl_init

<!-- snip... -->

[openssl_init]
engines=engine_section

[engine_section]
pkcs11 = pkcs11_section

[pkcs11_section]
engine_id = pkcs11
dynamic_path = /Users/tobyealden/code/tdxvolt-openssl-3/openssl-release/lib/engines-3/libpkcs11.dylib
MODULE_PATH = /opt/homebrew/Cellar/opensc/0.24.0/lib/opensc-pkcs11.so
init = 0
```