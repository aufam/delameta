#include <iostream>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

void initialize_openssl() {
    SSL_load_error_strings();   
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() {
    EVP_cleanup();
}

SSL_CTX *create_context() {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_client_method();  // Use TLS_method() in newer versions
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void configure_context(SSL_CTX *ctx) {
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    if (!SSL_CTX_load_verify_locations(ctx, "path/to/cacert.pem", NULL)) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

int create_socket(const char* hostname, int port) {
    struct hostent *host;
    struct sockaddr_in addr;
    int sock;

    host = gethostbyname(hostname);
    if (!host) {
        perror("Unable to resolve host");
        exit(EXIT_FAILURE);
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = *(long*)(host->h_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        perror("Unable to connect to server");
        close(sock);
        exit(EXIT_FAILURE);
    }
    return sock;
}

void perform_request(SSL *ssl) {
    const char* request = "GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n";
    SSL_write(ssl, request, strlen(request));

    char buffer[2048];
    int bytes;
    while ((bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes] = '\0';
        std::cout << buffer;
    }
}

int example_main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <hostname> <port>\n";
        return 1;
    }

    const char* hostname = argv[1];
    int port = atoi(argv[2]);

    initialize_openssl();
    SSL_CTX *ctx = create_context();
    configure_context(ctx);

    int sock = create_socket(hostname, port);
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);

    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
    } else {
        std::cout << "Connected with " << SSL_get_cipher(ssl) << " encryption\n";
        perform_request(ssl);
    }

    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    cleanup_openssl();
    return 0;
}
