#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct {
    int use_ssl;
    char host[256];
    int port;
    char path[1024];
} url_parts;

static void parse_url(const char *url, url_parts *u) {
    memset(u, 0, sizeof(*u));
    u->use_ssl = 1;            // default https
    u->port = 443;

    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) {
        u->use_ssl = 0;
        u->port = 80;
        p += 7;
    } else if (strncmp(p, "https://", 8) == 0) {
        u->use_ssl = 1;
        u->port = 443;
        p += 8;
    }

    const char *slash = strchr(p, '/');
    const char *hostend = slash ? slash : p + strlen(p);

    const char *colon = memchr(p, ':', hostend - p);
    if (colon) {
        size_t hlen = colon - p;
        if (hlen >= sizeof(u->host)) hlen = sizeof(u->host)-1;
        memcpy(u->host, p, hlen);
        u->host[hlen] = '\0';
        u->port = atoi(colon + 1);
    } else {
        size_t hlen = hostend - p;
        if (hlen >= sizeof(u->host)) hlen = sizeof(u->host)-1;
        memcpy(u->host, p, hlen);
        u->host[hlen] = '\0';
    }

    if (slash) {
        snprintf(u->path, sizeof(u->path), "%s", slash);
    } else {
        strcpy(u->path, "/");
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <url> [-o file]\n", argv[0]);
        return 1;
    }

    url_parts u;
    parse_url(argv[1], &u);

    const char *outfile = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i+1 < argc) {
            outfile = argv[++i];
        }
    }

    struct hostent *host = gethostbyname(u.host);
    if (!host) {
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(u.port);
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

    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;
    if (u.use_ssl) {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
        const SSL_METHOD *method = TLS_client_method();
        ctx = SSL_CTX_new(method);
        if (!ctx) {
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }
        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, sock);
        if (SSL_connect(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            close(sock);
            SSL_CTX_free(ctx);
            exit(EXIT_FAILURE);
        }
    }

    char request[2048];
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n\r\n",
        u.path, u.host);

    FILE *out = stdout;
    if (outfile) {
        out = fopen(outfile, "wb");
        if (!out) {
            perror("fopen");
            if (ssl) SSL_free(ssl);
            close(sock);
            if (ctx) SSL_CTX_free(ctx);
            return 1;
        }
    }

    /* send request */
    int len = strlen(request);
    int written = 0;
    while (written < len) {
        int n = u.use_ssl ? SSL_write(ssl, request + written, len - written)
                          : send(sock, request + written, len - written, 0);
        if (n <= 0) {
            ERR_print_errors_fp(stderr);
            break;
        }
        written += n;
    }

    /* read and skip headers */
    int header_done = 0;
    char header_buf[8192];
    size_t header_buf_len = 0;
    char buffer[4096];

    for (;;) {
        int bytes = u.use_ssl ? SSL_read(ssl, buffer, sizeof(buffer))
                              : recv(sock, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;

        if (!header_done) {
            /* accumulate headers */
            if (header_buf_len + bytes > sizeof(header_buf)) {
                fprintf(stderr, "headers too large\n");
                break;
            }
            memcpy(header_buf + header_buf_len, buffer, bytes);
            header_buf_len += bytes;

            /* find end of headers */
            if (header_buf_len >= 4) {
                void *hdr_end = memmem(header_buf, header_buf_len, "\r\n\r\n", 4);
                if (hdr_end) {
                    header_done = 1;
                    size_t header_len = (unsigned char*)hdr_end + 4 - (unsigned char*)header_buf;
                    size_t body_len = header_buf_len - header_len;
                    if (body_len > 0)
                        fwrite(header_buf + header_len, 1, body_len, out);
                }
            }
        } else {
            fwrite(buffer, 1, bytes, out);
        }
    }

    if (outfile) fclose(out);
    if (u.use_ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    close(sock);
    if (ctx) SSL_CTX_free(ctx);
    return 0;
}
