// File: gNodeB.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define UDP_PORT 5000
#define TCP_PORT 6000
#define UE_IP "127.0.0.1"

int paging_message[4];
int has_paging = 0;

void* tcp_server_thread(void* arg) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, 5);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        recv(client_fd, paging_message, sizeof(paging_message), 0);
        printf("[gNodeB] Received NgAP Paging: ue_id = %d\n", paging_message[1]);
        has_paging = 1;
        close(client_fd);
    }
    return NULL;
}

int main() {
    pthread_t tcp_thread;
    pthread_create(&tcp_thread, NULL, tcp_server_thread, NULL);

    int udp_sockfd;
    struct sockaddr_in ue_addr;
    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    ue_addr.sin_family = AF_INET;
    ue_addr.sin_port = htons(UDP_PORT);
    ue_addr.sin_addr.s_addr = inet_addr(UE_IP);

    while (1) {
        usleep(10000); // 10ms

        if (has_paging) {
            sendto(udp_sockfd, paging_message, sizeof(paging_message), 0,
                   (struct sockaddr*)&ue_addr, sizeof(ue_addr));
            printf("[gNodeB] Sent RRC Paging to UE\n");
            has_paging = 0;
        }
    }

    close(udp_sockfd);
    return 0;
}