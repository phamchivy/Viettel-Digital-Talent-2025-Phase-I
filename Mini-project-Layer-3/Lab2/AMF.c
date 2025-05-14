// File: AMF.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 6000

int main() {
    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    int message[4] = {100, 12345, 100, 100}; // message_type, ue_id, tac, cn_domain
    send(sockfd, message, sizeof(message), 0);

    printf("[AMF] Sent NgAP Paging message\n");
    close(sockfd);
    return 0;
}