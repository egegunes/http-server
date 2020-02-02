#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#define LISTEN_PORT 6666
#define REQ_SIZE 80000

ssize_t read_request(int fd, void *buffer, size_t n) {
    ssize_t n_read;
    ssize_t t_read;
    char    *buf;
    char    ch;
    char    lch;

    buf = buffer;

    t_read = 0;
    for (;;) {
        n_read = read(fd, &ch, 1);

        if (n_read == -1) {
            return -1;
        } else if (n_read == 0) {
            if (t_read == 0) {
                return 0;
            }

            break;
        } else if (t_read < n - 1) {
            t_read++;
            *buf++ = ch;
        }

        if (ch == '\r' && lch == '\n') {
            break;
        }

        lch = ch;
    }

    *buf = '\0';
    return t_read;
}

ssize_t write_response(int fd) {
    ssize_t  n_write;
    char    *s_line = "HTTP/1.1 200 OK\r\nHost: 0.0.0.0:6666\r\n\r\n";

    n_write = write(fd, s_line, strlen(s_line));

    return n_write;
}

int main() {
    int sfd, cfd;
    struct sockaddr_in my_addr, peer_addr;
    socklen_t peer_addr_size;

    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int reuseaddr_val = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_val, (socklen_t) sizeof(SO_REUSEADDR)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr.sin_port = htons(LISTEN_PORT);

    if (bind(sfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr_in)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(sfd, 50) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    peer_addr_size = sizeof(struct sockaddr_in);
    cfd = accept(sfd, (struct sockaddr *) &peer_addr, &peer_addr_size);
    if (cfd == -1) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    char req[REQ_SIZE];
    ssize_t n_read = read_request(cfd, req, REQ_SIZE);
    if (n_read < 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    printf("%s\n", req);

    ssize_t n_write = write_response(cfd);
    if (n_write < 0) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    printf("%ld\n", n_write);

    close(sfd);
    close(cfd);

    return 0;
}
