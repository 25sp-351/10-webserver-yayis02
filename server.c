#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#define DEFAULT_PORT 80
#define BUFFER_SIZE 8192

void set_content_type(const char *path, char *type_buf) {
    if (strstr(path, ".html")) strcpy(type_buf, "text/html");
    else if (strstr(path, ".png")) strcpy(type_buf, "image/png");
    else if (strstr(path, ".jpg") || strstr(path, ".jpeg")) strcpy(type_buf, "image/jpeg");
    else if (strstr(path, ".css")) strcpy(type_buf, "text/css");
    else if (strstr(path, ".js")) strcpy(type_buf, "application/javascript");
    else strcpy(type_buf, "application/octet-stream");
}

void send_response(int client_fd, int status, const char *content_type, const char *body, size_t body_len) {
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: keep-alive\r\n\r\n",
             status, content_type, body_len);
    send(client_fd, header, strlen(header), 0);
    send(client_fd, body, body_len, 0);
}

void handle_static(int client_fd, const char *path) {
    char file_path[1024] = "./static";
    strcat(file_path, path + strlen("/static"));

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        const char *msg = "File not found";
        send_response(client_fd, 404, "text/plain", msg, strlen(msg));
        return;
    }

    struct stat st;
    fstat(fd, &st);
    size_t file_size = st.st_size;
    char *file_buf = malloc(file_size);
    read(fd, file_buf, file_size);
    close(fd);

    char content_type[64];
    set_content_type(file_path, content_type);
    send_response(client_fd, 200, content_type, file_buf, file_size);
    free(file_buf);
}

void handle_calc(int client_fd, const char *path) {
    char op[8];
    int a, b;
    if (sscanf(path, "/calc/%7[^/]/%d/%d", op, &a, &b) != 3) {
        const char *msg = "Invalid format";
        send_response(client_fd, 400, "text/plain", msg, strlen(msg));
        return;
    }

    char result[128];
    if (strcmp(op, "add") == 0) sprintf(result, "%d", a + b);
    else if (strcmp(op, "mul") == 0) sprintf(result, "%d", a * b);
    else if (strcmp(op, "div") == 0) {
        if (b == 0) {
            const char *msg = "Divide by zero";
            send_response(client_fd, 400, "text/plain", msg, strlen(msg));
            return;
        }
        sprintf(result, "%d", a / b);
    } else {
        const char *msg = "Unknown operation";
        send_response(client_fd, 400, "text/plain", msg, strlen(msg));
        return;
    }

    send_response(client_fd, 200, "text/plain", result, strlen(result));
}

void handle_sleep(int client_fd, const char *path) {
    int seconds;
    if (sscanf(path, "/sleep/%d", &seconds) != 1) {
        const char *msg = "Invalid format";
        send_response(client_fd, 400, "text/plain", msg, strlen(msg));
        return;
    }

    sleep(seconds);
    const char *msg = "Slept!";
    send_response(client_fd, 200, "text/plain", msg, strlen(msg));
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    int bytes_read = 0;

    while (1) {
        int bytes = recv(client_fd, buffer + bytes_read, sizeof(buffer) - bytes_read - 1, 0);
        if (bytes <= 0) break;
        bytes_read += bytes;
        buffer[bytes_read] = '\0';

        // Process all pipelined requests in buffer
        char *req_start = buffer;
        while (1) {
            char *req_end = strstr(req_start, "\r\n\r\n");
            if (!req_end) break;

            size_t req_len = req_end - req_start + 4;
            char req_copy[BUFFER_SIZE];
            strncpy(req_copy, req_start, req_len);
            req_copy[req_len] = '\0';

            char method[8], path[1024], version[16];
            sscanf(req_copy, "%7s %1023s %15s", method, path, version);

            if (strcmp(method, "GET") != 0) {
                const char *msg = "Only GET supported";
                send_response(client_fd, 405, "text/plain", msg, strlen(msg));
            } else if (strncmp(path, "/static/", 8) == 0) {
                handle_static(client_fd, path);
            } else if (strncmp(path, "/calc/", 6) == 0) {
                handle_calc(client_fd, path);
            } else if (strncmp(path, "/sleep/", 7) == 0) {
                handle_sleep(client_fd, path);
            } else {
                const char *msg = "Not Found";
                send_response(client_fd, 404, "text/plain", msg, strlen(msg));
            }

            req_start = req_end + 4;
        }

        // Move leftover to beginning of buffer
        size_t remaining = buffer + bytes_read - req_start;
        memmove(buffer, req_start, remaining);
        bytes_read = remaining;
    }

    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        port = atoi(argv[2]);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };
    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 10);

    printf("Server running on port %d\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_fd);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
