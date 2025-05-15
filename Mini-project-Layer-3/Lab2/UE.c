// File: UE.c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define UDP_PORT 5000
#define UDP_PORT_SYNC 8888

unsigned short UE_sfn = 0;

void* sfn_sync_thread(void* arg) {
    int sockfd;
    struct sockaddr_in my_addr, sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(UDP_PORT_SYNC);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sockfd, (struct sockaddr*)&my_addr, sizeof(my_addr));

    bool synced = false;
    int sync_counter = 0;

    while (1) {
        usleep(10000); // mỗi 10ms
        UE_sfn = (UE_sfn + 1) % 1024;

        unsigned char buffer[3];
        int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), MSG_DONTWAIT, (struct sockaddr*)&sender_addr, &addr_len);
        if (bytes_received == 3 && buffer[0] == 0x01) {
            unsigned short received_sfn = (buffer[1] << 8) | buffer[2];

            sync_counter += 80;

            if (!synced || sync_counter >= 800) {
                UE_sfn = received_sfn;
                printf("[UE] Synced SFN = %d\n", UE_sfn);
                sync_counter = 0;
                synced = true;

                // Gửi ACK về gNodeB nếu cần (tùy hệ thống)
            }
        }
    }

    close(sockfd);
    return NULL;
}

void* paging_receiver_thread(void* arg) {
    int sockfd;
    struct sockaddr_in my_addr;
    socklen_t addr_len = sizeof(my_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(UDP_PORT);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sockfd, (struct sockaddr*)&my_addr, sizeof(my_addr));

    unsigned char buffer[3];
    while (1) {
        int n = recvfrom(sockfd, buffer, sizeof(buffer), MSG_DONTWAIT,
                         (struct sockaddr*)&my_addr, &addr_len);

        if (n == 3 && buffer[0] == 0x64) {
            const char* domain_type = (buffer[2] == 100) ? "VoLTE" :
                                      (buffer[2] == 101) ? "Data" : "Unknown";
            printf("[UE] SFN = %d | Received RRC Paging: ue_id = %d, cn_domain = %s\n",
                   UE_sfn, buffer[1], domain_type);
        }
    }

    close(sockfd);
    return NULL;
}

int main() {
    pthread_t thread_sfn_sync, thread_receiver;

    pthread_create(&thread_sfn_sync, NULL, sfn_sync_thread, NULL);
    pthread_create(&thread_receiver, NULL, paging_receiver_thread, NULL);

    pthread_join(thread_sfn_sync, NULL);
    pthread_join(thread_receiver, NULL);

    return 0;
}
