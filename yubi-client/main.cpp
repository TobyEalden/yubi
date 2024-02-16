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

        // You can now use SSL_read() and SSL_write() to communicate securely
        // For example: SSL_write(ssl, "message", strlen("message"));

        // Send the HTTP POST request
        char request[1024];
//        sprintf(request, "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %lu\r\n\r\n%s", path, hostname, strlen(data), data);
        sprintf(request, "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %lu\r\n\r\n%s", path, hostname, strlen(data), data);

        SSL_write(ssl, request, strlen(request));


        // Close the SSL connection
        SSL_shutdown(ssl);
    }

    SSL_free(ssl);
    close(server_fd); // Close the socket

    return 0;
}

int main() {
    // Initialize OpenSSL
//    SSL_library_init();
//    OpenSSL_add_all_algorithms();
//    SSL_load_error_strings();

    const char* engine_id = "pkcs11";
    const char* module_path = "/usr/local/lib/libykcs11.dylib"; // Path to the OpenSC PKCS#11 module
    const char* pin = "123456"; // Your YubiKey's PIN
    const char* cert_id = "01"; // ID of the certificate object on the YubiKey

    // Load the pkcs11 engine
    ENGINE_load_dynamic();
    ENGINE* pkcs11_engine = ENGINE_by_id(engine_id);
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
    auto* ctx = create_context();

    // Load the client certificate from the YubiKey
    if (!SSL_CTX_use_certificate_file(ctx, "/Users/tobyealden/code/yubi-01/client-cert.pem", SSL_FILETYPE_PEM)) {
        std::cerr << "Failed to load certificate" << std::endl;
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        ENGINE_finish(pkcs11_engine);
        ENGINE_free(pkcs11_engine);
        exit(1);
    }

    // Associate the private key from the YubiKey with the SSL context
    EVP_PKEY* pkey = ENGINE_load_private_key(pkcs11_engine, cert_id, NULL, NULL);
    if (!pkey) {
        std::cerr << "Failed to load private key" << std::endl;
        SSL_CTX_free(ctx);
        ENGINE_finish(pkcs11_engine);
        ENGINE_free(pkcs11_engine);
        exit(1);
    }

    if (!SSL_CTX_use_PrivateKey(ctx, pkey)) {
        std::cerr << "Failed to use private key" << std::endl;
        EVP_PKEY_free(pkey);
        SSL_CTX_free(ctx);
        ENGINE_finish(pkcs11_engine);
        ENGINE_free(pkcs11_engine);
        exit(1);
    }

    // Verify private key
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

    // Set the 'operand' field in the POST body data and encode for a POST request
    auto body = "{\"operand\": 2}";

    sendHTTPPOST(ssl, "127.0.0.1", "4443", "/test", body);

    // Clean up
    EVP_PKEY_free(pkey);
    SSL_CTX_free(ctx);
    ENGINE_finish(pkcs11_engine);
    ENGINE_free(pkcs11_engine);

    return 0;
}
