#ifndef __REQUEST_H__

#define MAXBUF 8192

void request_handle(int fd, char method[MAXBUF], char uri[MAXBUF],
                    char version[MAXBUF]);
void request_error(int fd, char *cause, char *errnum, char *shortmsg,
                   char *longmsg);
int request_parse_uri(char *uri, char *filename, char *cgiargs);

#endif // __REQUEST_H__
