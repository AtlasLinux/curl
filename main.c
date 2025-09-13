#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <IP>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80); // default HTTP port

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(fd);
        return 1;
    }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return 1;
    }

    printf("Connected to %s!\n", ip);

    // send simple HTTP GET
    const char *request = "GET / HTTP/1.0\r\n\r\n";
    if (write(fd, request, strlen(request)) < 0) {
        perror("write");
        close(fd);
        return 1;
    }

    char buf[512];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf)-1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }

    if (n < 0) perror("read");

    close(fd);
    return 0;
}
