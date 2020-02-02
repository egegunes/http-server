#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CR (u_char)'\r'
#define LF (u_char)'\n'
#define NULLCHAR (u_char)'\0'
#define LISTEN_PORT 6666
#define REQ_SIZE 80000

struct http_request_s {
  char *method;
  char *path;
  char *http_version;
};

ssize_t read_request(int fd, void *buffer, size_t n) {
  ssize_t n_read;
  ssize_t t_read;
  char *buf;
  char ch;
  char lch;

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

    if (ch == CR && lch == LF) {
      break;
    }

    lch = ch;
  }

  *buf = NULLCHAR;
  return t_read;
}

int parse_request(char *req) {
  size_t max_groups = 4;

  int ret;
  unsigned int req_line_offset;
  regex_t regex;
  regmatch_t groups[max_groups];

  struct http_request_s request;

  ret = regcomp(
      &regex,
      "([A-Z]+)[[:blank:]]([\\/a-zA-Z0-9.]+)[[:blank:]]HTTP\\/([0-9.]+)",
      REG_EXTENDED);
  if (ret != 0) {
    return ret;
  }

  ret = regexec(&regex, req, max_groups, groups, 0);
  if (ret != 0) {
    return ret;
  }

  req_line_offset = groups[0].rm_eo;
  printf("Request line offset: %d\n\n", req_line_offset);

  request.method = req + groups[1].rm_so;
  request.method[groups[1].rm_eo] = NULLCHAR;
  printf("Method: %s\n\n", request.method);

  request.path = req + groups[2].rm_so;
  request.path[groups[2].rm_eo] = NULLCHAR;
  printf("Path: %s\n\n", request.path);

  request.http_version = req + groups[3].rm_so;
  request.http_version[groups[3].rm_eo] = NULLCHAR;
  printf("HTTP Version: %s\n\n", request.http_version);

  regfree(&regex);

  return ret;
}

ssize_t write_response(int fd) {
  ssize_t n_write;
  char *s_line = "HTTP/1.1 200 OK\r\nHost: 0.0.0.0:6666\r\n\r\n";

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
  if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_val,
                 (socklen_t)sizeof(SO_REUSEADDR)) == -1) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  my_addr.sin_family = AF_INET;
  my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  my_addr.sin_port = htons(LISTEN_PORT);

  if (bind(sfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr_in)) ==
      -1) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  if (listen(sfd, 50) == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  peer_addr_size = sizeof(struct sockaddr_in);
  cfd = accept(sfd, (struct sockaddr *)&peer_addr, &peer_addr_size);
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

  int regex_ret = parse_request(req);
  if (regex_ret == 0) {
    printf("Match!\n");
  } else if (regex_ret == REG_NOMATCH) {
    printf("No match!\n");
  }

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
