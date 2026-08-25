/*
 * SzpontOS - /bin/httpd Freestanding Micro Web Server
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sysinfo.h>

static void handle_client(int cfd) {
    char req[1024];
    ssize_t n = recv(cfd, req, sizeof(req) - 1, 0);
    if (n <= 0) {
        close(cfd);
        return;
    }
    req[n] = '\0';

    struct sysinfo si;
    memset(&si, 0, sizeof(si));
    sysinfo(&si);

    char body[2048];
    int body_len = snprintf(body, sizeof(body),
                            "<!DOCTYPE html>\n"
                            "<html>\n"
                            "<head>\n"
                            "<meta charset=\"utf-8\">\n"
                            "<title>SzpontOS Web Dashboard</title>\n"
                            "<style>\n"
                            "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; "
                            "background: #0f172a; color: #f8fafc; padding: 2rem; }\n"
                            ".card { background: #1e293b; border-radius: 12px; padding: 1.5rem; max-width: 600px; "
                            "margin: 0 auto; box-shadow: 0 10px 25px rgba(0,0,0,0.5); border: 1px solid #334155; }\n"
                            "h1 { color: #38bdf8; margin-top: 0; font-size: 1.75rem; }\n"
                            ".badge { display: inline-block; background: #0284c7; color: white; padding: 0.25rem "
                            "0.75rem; border-radius: 9999px; font-size: 0.85rem; font-weight: bold; }\n"
                            "p { margin: 0.5rem 0; line-height: 1.5; color: #cbd5e1; }\n"
                            ".stat { color: #a855f7; font-weight: bold; }\n"
                            "</style>\n"
                            "</head>\n"
                            "<body>\n"
                            "<div class=\"card\">\n"
                            "  <h1>SzpontOS HTTP Daemon</h1>\n"
                            "  <span class=\"badge\">Kernel: x86_64 Monolithic</span>\n"
                            "  <p style=\"margin-top: 1rem;\">Serving HTTP requests directly from Ring 3 userland with "
                            "native Berkeley Sockets & TCP/IP stack.</p>\n"
                            "  <hr style=\"border-color: #334155; margin: 1rem 0;\">\n"
                            "  <p>System Uptime: <span class=\"stat\">%ld seconds</span></p>\n"
                            "  <p>Total Memory: <span class=\"stat\">%lu MB</span></p>\n"
                            "  <p>Free Memory: <span class=\"stat\">%lu MB</span></p>\n"
                            "  <p>Active Tasks: <span class=\"stat\">%u</span></p>\n"
                            "  <p style=\"font-size: 0.8rem; color: #64748b; margin-top: 1.5rem;\">&copy; Copyright by "
                            "Szpont Industries. All rights reserved.</p>\n"
                            "</div>\n"
                            "</body>\n"
                            "</html>\n",
                            si.uptime, si.totalram / (1024 * 1024), si.freeram / (1024 * 1024), si.procs);

    char header[512];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/html; charset=UTF-8\r\n"
                              "Content-Length: %d\r\n"
                              "Connection: close\r\n"
                              "Server: SzpontOS-Httpd/1.0\r\n\r\n",
                              body_len);

    send(cfd, header, (size_t)header_len, 0);
    send(cfd, body, (size_t)body_len, 0);
    close(cfd);
}

int main(int argc, char **argv) {
    int port = 80;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535)
            port = 80;
    }

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        perror("httpd: socket");
        return 1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons((uint16_t)port);

    if (bind(sfd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("httpd: bind");
        close(sfd);
        return 1;
    }

    if (listen(sfd, 10) < 0) {
        perror("httpd: listen");
        close(sfd);
        return 1;
    }

    printf("SzpontOS httpd daemon listening on http://0.0.0.0:%d/\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t clen = sizeof(client_addr);
        int cfd = accept(sfd, (struct sockaddr *)&client_addr, &clen);
        if (cfd >= 0) {
            handle_client(cfd);
        }
    }

    close(sfd);
    return 0;
}
