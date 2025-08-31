#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define MAX_TODOS 100

typedef struct
{
    int id;
    char text[256];
} Todo;

Todo todos[MAX_TODOS];
int todo_count = 0;
int next_id = 1;

// Helper: send HTTP response
void send_response(SOCKET client, const char *content_type, const char *body)
{
    char header[512];
    sprintf(header,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "\r\n",
            content_type);
    send(client, header, (int)strlen(header), 0);
    send(client, body, (int)strlen(body), 0);
}

// Handle API and static files
void handle_request(SOCKET client, const char *req)
{
    char method[8], path[1024];
    sscanf(req, "%s %s", method, path);

    // ---- API: /api/todos ----
    if (strncmp(path, "/api/todos", 10) == 0)
    {
        if (strcmp(method, "GET") == 0)
        {
            char buffer[5000] = "[";
            for (int i = 0; i < todo_count; i++)
            {
                char item[512];
                sprintf(item, "{\"id\":%d,\"text\":\"%s\"}%s",
                        todos[i].id, todos[i].text,
                        (i == todo_count - 1 ? "" : ","));
                strcat(buffer, item);
            }
            strcat(buffer, "]");
            send_response(client, "application/json", buffer);
        }
        else if (strcmp(method, "POST") == 0)
        {
            const char *body = strstr(req, "\r\n\r\n");
            if (body)
                body += 4;
            if (todo_count < MAX_TODOS)
            {
                todos[todo_count].id = next_id++;
                strncpy(todos[todo_count].text, body, 255);
                todos[todo_count].text[strcspn(todos[todo_count].text, "\r\n")] = 0;
                todo_count++;
            }
            send_response(client, "application/json", "{\"status\":\"created\"}");
        }
        else if (strcmp(method, "PUT") == 0)
        {
            int id;
            sscanf(path, "/api/todos/%d", &id);
            const char *body = strstr(req, "\r\n\r\n");
            if (body)
                body += 4;
            for (int i = 0; i < todo_count; i++)
            {
                if (todos[i].id == id)
                {
                    strncpy(todos[i].text, body, 255);
                    todos[i].text[strcspn(todos[i].text, "\r\n")] = 0;
                    break;
                }
            }
            send_response(client, "application/json", "{\"status\":\"updated\"}");
        }
        else if (strcmp(method, "DELETE") == 0)
        {
            int id;
            sscanf(path, "/api/todos/%d", &id);
            for (int i = 0; i < todo_count; i++)
            {
                if (todos[i].id == id)
                {
                    for (int j = i; j < todo_count - 1; j++)
                        todos[j] = todos[j + 1];
                    todo_count--;
                    break;
                }
            }
            send_response(client, "application/json", "{\"status\":\"deleted\"}");
        }
        else if (strcmp(method, "OPTIONS") == 0)
        {
            send_response(client, "text/plain", "");
        }
    }
    // ---- Static files ----
    else
    {
        char file_path[1024];
        if (strcmp(path, "/") == 0)
            strcpy(file_path, "index.html");
        else
            strcpy(file_path, path + 1); // remove leading '/'

        FILE *file = fopen(file_path, "rb");
        if (!file)
        {
            send_response(client, "text/plain", "404 Not Found");
            return;
        }

        fseek(file, 0, SEEK_END);
        long fsize = ftell(file);
        rewind(file);

        char *content = malloc(fsize + 1);
        fread(content, 1, fsize, file);
        content[fsize] = 0;
        fclose(file);

        // Detect content type
        const char *ctype = "text/plain";
        if (strstr(file_path, ".html"))
            ctype = "text/html";
        else if (strstr(file_path, ".css"))
            ctype = "text/css";

        send_response(client, ctype, content);
        free(content);
    }
}

int main()
{
    WSADATA wsa;
    SOCKET server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[30000];

    // Init Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET)
    {
        printf("Socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR)
    {
        printf("Bind failed: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    listen(server_fd, 10);
    printf("Server running at http://localhost:%d\n", PORT);

    while (1)
    {
        client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (client_fd == INVALID_SOCKET)
            continue;

        int valread = recv(client_fd, buffer, sizeof(buffer), 0);
        if (valread > 0)
        {
            buffer[valread] = '\0';
            handle_request(client_fd, buffer);
        }
        closesocket(client_fd);
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}
