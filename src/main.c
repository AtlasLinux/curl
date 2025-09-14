#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <host> [path] [-p port] [-o file]\n", argv[0]);
        return 1;
    }

    const char* hostname = NULL;
    const char* path = "/";
    int port = 443;
    const char* outfile = NULL;

    // parse args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i+1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) {
            outfile = argv[++i];
        } else if (!hostname) {
            hostname = argv[i];
        } else {
            path = argv[i];
        }
    }

    if (!hostname) {
        fprintf(stderr, "No host specified\n");
        return 1;
    }

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    const SSL_METHOD* method = TLS_client_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    struct hostent *host = gethostbyname(hostname);
    if (!host) {
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port); // HTTPS port
    memcpy(&addr.sin_addr.s_addr, host->h_addr, host->h_length);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        perror("connect");
        close(sock);
        exit(EXIT_FAILURE);
    }
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);

    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(sock);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }

    printf("Connected with %s encryption\n", SSL_get_cipher(ssl));
    char request[512];
    snprintf(request, sizeof(request), 
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n", path, hostname);

    FILE* out = stdout;
    if (outfile) {
        out = fopen(outfile, "w");
        if (!out) {
            perror("fopen");
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(sock);
            SSL_CTX_free(ctx);
            return 1;
        }
    }

    int written = 0;
    int len = strlen(request);
    while (written < len) {
        int n = SSL_write(ssl, request + written, len - written);
        if (n <= 0) {
            ERR_print_errors_fp(stderr);
            break;
        }
        written += n;
    }
    char buffer[4096];
    int bytes;
    while ((bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes] = '\0';
        fwrite(buffer, 1, bytes, out);
    }

    if (outfile) fclose(out);
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    return 0;
}