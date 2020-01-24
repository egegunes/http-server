#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#define LISTEN_PORT 6666

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

    printf("%d connected: %s\n", cfd, inet_ntoa(peer_addr.sin_addr));

    char buff[8000];
    while(read(cfd, &buff, sizeof(buff)) > 0) {
        printf("%s", buff);
    }

    close(sfd);
    close(cfd);

    return 0;
}
