#include <iostream>
#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <openssl/provider.h>

int create_socket(const char *hostname, const char *port) {
    int sockfd;
    struct sockaddr_in server_addr;

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Cannot create socket");
        exit(EXIT_FAILURE);
    }

    // Convert port from string to int
    int portno = atoi(port);

    // Configure server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portno);

    // Convert hostname to IP address and set in server_addr
    if (inet_pton(AF_INET, hostname, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

SSL_CTX *create_context() {
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

int sendHTTPPOST(SSL *ssl, const char *hostname, const char *port, const char *path, const char *data) {
    int server_fd = create_socket(hostname, port);

    SSL_set_fd(ssl, server_fd);
    if (SSL_connect(ssl) != 1) {
        ERR_print_errors_fp(stderr);
    } else {
        printf("Connected with %s encryption\n", SSL_get_cipher(ssl));

        // Send the HTTP POST request
        char request[1024];
        sprintf(request,
                "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %lu\r\n\r\n%s", path,
                hostname, strlen(data), data);

        SSL_write(ssl, request, strlen(request));

        // Read the HTTP response
        char response[4096];
        int bytes = SSL_read(ssl, response, sizeof(response));
        response[bytes] = 0;
        printf("Server response: %s\n", response);

        // Close the SSL connection
        SSL_shutdown(ssl);
    }

    SSL_free(ssl);
    close(server_fd); // Close the socket

    return 0;
}

int main() {
    const char *engine_id = "pkcs11";

    // **LOOKOUT** - hard-coded path to the PKCS#11 module.
#ifdef __APPLE__
//    const char *module_path = "/usr/local/lib/libykcs11.dylib";
    const char *module_path = "/opt/homebrew/Cellar/opensc/0.24.0/lib/opensc-pkcs11.so";
#elifdef __linux__
//    const char *module_path = "/usr/local/lib/libykcs11.so";
    const char *module_path = "/usr/lib/arm-linux-gnueabihf/opensc-pkcs11.so";
#elifdef _WIN32
    #error "Unsupported platform"
#endif

    // YubiKey's PIN - this is the default.
    const char *pin = "123456";

    // ID of the 9a slot on the YubiKey
    const char *cert_id = "01";

    // Load the pkcs11 engine. This is marked as deprecated in 3.x, but the
    // `libp11` library we're using does not support the new `provider` API.
    ENGINE_load_dynamic();
    ENGINE *pkcs11_engine = ENGINE_by_id(engine_id);
    if (!pkcs11_engine) {
        std::cerr << "Could not load engine" << std::endl;
        exit(1);
    }

    // Initialize the engine
    if (!ENGINE_ctrl_cmd_string(pkcs11_engine, "MODULE_PATH", module_path, 0) ||
        !ENGINE_ctrl_cmd_string(pkcs11_engine, "PIN", pin, 0) ||
        !ENGINE_init(pkcs11_engine)) {
        std::cerr << "Could not initialize engine" << std::endl;
        ENGINE_free(pkcs11_engine);
        exit(1);
    }

    // Create a new SSL context
    auto *ctx = create_context();

    // Load the client certificate from the local disk.
    if (!SSL_CTX_use_certificate_file(ctx, "./client-cert.pem", SSL_FILETYPE_PEM)) {
        std::cerr << "Failed to load certificate" << std::endl;
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        ENGINE_finish(pkcs11_engine);
        ENGINE_free(pkcs11_engine);
        exit(1);
    }

    // Insist that the server we're connecting to has a certificate issued by `ca.pem`.
    if (!SSL_CTX_load_verify_locations(ctx, "./ca.pem", NULL)) {
        std::cerr << "Failed to load CA file" << std::endl;
        SSL_CTX_free(ctx);
        ENGINE_finish(pkcs11_engine);
        ENGINE_free(pkcs11_engine);
        exit(1);
    }

    // Set the verification mode to require a certificate from the server.
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);

    // Load the private key from the YubiKey.
    EVP_PKEY *pkey = ENGINE_load_private_key(pkcs11_engine, cert_id, NULL, NULL);
    if (!pkey) {
        std::cerr << "Failed to load private key" << std::endl;
        SSL_CTX_free(ctx);
        ENGINE_finish(pkcs11_engine);
        ENGINE_free(pkcs11_engine);
        exit(1);
    }

    // Associate it with the SSL context.
    if (!SSL_CTX_use_PrivateKey(ctx, pkey)) {
        std::cerr << "Failed to use private key" << std::endl;
        EVP_PKEY_free(pkey);
        SSL_CTX_free(ctx);
        ENGINE_finish(pkcs11_engine);
        ENGINE_free(pkcs11_engine);
        exit(1);
    }

    // Verify private key matches the client certificate.
    if (!SSL_CTX_check_private_key(ctx)) {
        std::cerr << "Private key does not match the public certificate" << std::endl;
        EVP_PKEY_free(pkey);
        SSL_CTX_free(ctx);
        ENGINE_finish(pkcs11_engine);
        ENGINE_free(pkcs11_engine);
        exit(1);
    }

    // Proceed with creating and setting up the SSL connection...
    auto ssl = SSL_new(ctx);

    // Set the 'operand' field in the POST body data for the /test endpoint.
    auto body = "{\"operand\": 2}";

    // Send the HTTP POST request to the server (see `yubi-server`).
    sendHTTPPOST(ssl, "127.0.0.1", "4443", "/test", body);

    // Clean up
    EVP_PKEY_free(pkey);
    SSL_CTX_free(ctx);
    ENGINE_finish(pkcs11_engine);
    ENGINE_free(pkcs11_engine);

    return 0;
}
