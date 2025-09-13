#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <host> [path] [-p port] [-o file]\n", argv[0]);
        return 1;
    }

    const char *host = NULL;
    const char *path = "/";
    int port = 80;
    const char *outfile = NULL;

    // parse args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i+1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) {
            outfile = argv[++i];
        } else if (!host) {
            host = argv[i];
        } else {
            path = argv[i];
        }
    }

    if (!host) {
        fprintf(stderr, "No host specified\n");
        return 1;
    }

    struct hostent *he = gethostbyname(host);
    if (!he) {
        fprintf(stderr, "Failed to resolve %s\n", host);
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return 1;
    }

    char request[512];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "\r\n", path, host);

    if (write(fd, request, strlen(request)) < 0) {
        perror("write");
        close(fd);
        return 1;
    }

    FILE *out = stdout;
    if (outfile) {
        out = fopen(outfile, "w");
        if (!out) {
            perror("fopen");
            close(fd);
            return 1;
        }
    }

    char buf[1024];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf)-1)) > 0) {
        fwrite(buf, 1, n, out);
    }
    if (n < 0) perror("read");

    if (outfile) fclose(out);
    close(fd);
    return 0;
}
