// UE.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define PORT 8888
#define SFN_MODULO 1024

int main() {
    int sockfd;
    struct sockaddr_in my_addr, sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // Bind socket to local address
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sockfd, (struct sockaddr*)&my_addr, sizeof(my_addr));

    unsigned short UE_sfn = 0;
    bool synced = false;
    int sync_counter = 0;

    while (1) {
        // Mỗi 10ms: tăng SFN
        usleep(10000);
        UE_sfn = (UE_sfn + 1) % SFN_MODULO;

        // Kiểm tra nếu có bản tin đến
        unsigned char buffer[3];
        int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), MSG_DONTWAIT, (struct sockaddr*)&sender_addr, &addr_len);
        if (bytes_received == 3 && buffer[0] == 0x01) {
            unsigned short received_sfn = (buffer[1] << 8) | buffer[2];

            sync_counter += 80; // Mỗi bản tin MIB đến sau 80ms

            if (!synced || sync_counter >= 800) {
                UE_sfn = received_sfn;
                printf("[UE] Synced SFN = %d\n", UE_sfn);
                sync_counter = 0;
                synced = true;
            } else {
                printf("[UE] Received MIB (SFN = %d) - no resync\n", received_sfn);
            }
        }

        // In ra UE_sfn
        static int log_timer = 0;
        log_timer += 10;
        if (log_timer >= 100) {
            printf("[UE] Current UE_sfn = %d\n", UE_sfn);
            log_timer = 0;
        }
    }

    close(sockfd);
    return 0;
}