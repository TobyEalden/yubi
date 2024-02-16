# yubico-piv-tool commands

The default PIN is 123456.

I had problems using the `yubico-piv-tool` on macOS after changing the default PIN.

## generate key

```
yubico-piv-tool -s 9a -a generate -o public.pem
``` 

Alternatively if you want to import an existing key:

```
yubico-piv-tool -s 9a -a import-key -i key.pem
```

## export public key

If you don't have the public key locally, you can export it from the YubiKey indirectly by first reading the certificate:

```
yubico-piv-tool -s 9a -a read-certificate -o cert.pem
```

and then using openssl to extract the public key from the certificate:

```
openssl x509 -in cert.pem -pubkey -noout > public.pem
```

## generate CSR

This requires the public key to be available in a file called `public.pem`. You may also want to change the subject information in the `-S` option.

```
yubico-piv-tool -a verify-pin -a request-certificate -s 9a -S '/CN=digi_sign/OU=test/O=example.com/' -i public.pem -o csr.pem
```

# yubi-server

This is a simple nodejs server that can be used to test the mutually authenticated flow.

When first run, it will generate a self-signed CA and then use it to issue a server certificate that will be used to back the HTTP server.

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

It requires a file named `csr.pem` (created using the `yubico-piv-tool` as described above), as well as the server's CA file `ca.pem`.

It simply sends the csr to the `/csr` endpoint and then saves the received certificate to a file named `client-cert.pem`.

# yubi-client

This is a test client that establishes an HTTPS connection to the `yubi-server` and sends a JSON request to the `/test` endpoint.

It requires that the client certificate is available in a file named `client-cert.pem` and the server CA is available in a file named `ca.pem`.

It then creates an SSL connection to the server at https://127.0.0.1:4443 using the private key stored on the yubikey.
