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
#define LISTEN_PORT 8000
#define REQ_SIZE 80000
#define MAX_REQ_GROUPS 4
#define MIN_HEADERS 1
#define MAX_HEADERS 100

typedef struct {
  char *key;
  char *value;
} http_header_t;

typedef struct {
  http_header_t *host;
} http_headers_in_t;

struct http_request_s {
  char *method;
  char *path;
  char *http_version;

  http_headers_in_t headers_in;
};

typedef struct http_request_s http_request_t;

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

int parse_request(http_request_t *r, char *req) {
  regex_t regex;
  regmatch_t req_groups[MAX_REQ_GROUPS];

  if (regcomp(
          &regex,
          "([A-Z]+)[[:blank:]]([\\/a-zA-Z0-9.]+)[[:blank:]]HTTP\\/([0-9.]+)",
          REG_EXTENDED) != 0) {
    // TODO: Error check
  }

  if (regexec(&regex, req, MAX_REQ_GROUPS, req_groups, 0) != 0) {
    // TODO: Error check
  }

  r->method = req + req_groups[1].rm_so;
  r->method[req_groups[1].rm_eo] = NULLCHAR;

  r->path = req + req_groups[2].rm_so;
  r->path[req_groups[2].rm_eo - req_groups[2].rm_so] = NULLCHAR;

  r->http_version = req + req_groups[3].rm_so;
  r->http_version[req_groups[3].rm_eo - req_groups[3].rm_so] = NULLCHAR;

  regfree(&regex);

  return 0;
}

char *read_file(char *filename) {
  char *buffer = 0;
  long length;

  printf("%s\n", filename);

  FILE *f = fopen(filename, "r");

  if (!f) {
    return buffer;
  }

  fseek(f, 0, SEEK_END);
  length = ftell(f);
  fseek(f, 0, SEEK_SET);
  buffer = malloc(length);

  if (buffer) {
    fread(buffer, 1, length, f);
  }

  fclose(f);

  return buffer;
}

ssize_t write_response(int fd, char *content) {
  ssize_t n_write;
  char *s_line = "HTTP/1.1 200 OK\r\n"
                 "Host: 0.0.0.0:8000\r\n"
                 "Server: lighteinte\r\n"
                 "Content-type: text/plain\r\n"
                 "Connection: close\r\n\r\n";

  n_write = write(fd, s_line, strlen(s_line));
  n_write += write(fd, content, strlen(content));

  return n_write;
}

ssize_t respond_404(int fd) {
  ssize_t n_write;
  char *s_line = "HTTP/1.1 404 Not Found\r\n"
                 "Host: 0.0.0.0:8000\r\n"
                 "Server: lighteinte\r\n"
                 "Content-type: text/plain\r\n"
                 "Connection: close\r\n\r\n";

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

  http_request_t *r = malloc(sizeof(*r));
  parse_request(r, req);
  printf("Method: %s\n", r->method);
  printf("Path: %s\n", r->path);
  printf("HTTP Version: %s\n", r->http_version);

  char *filename = ++r->path;
  char *content = read_file(filename);
  if (content == 0) {
    respond_404(cfd);
  } else {
    write_response(cfd, content);
  }

  shutdown(sfd, SHUT_RD);
  shutdown(cfd, SHUT_WR);

  return 0;
}
