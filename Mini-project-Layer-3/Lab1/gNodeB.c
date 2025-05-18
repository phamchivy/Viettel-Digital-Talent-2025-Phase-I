// gNodeB.c - Completed 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8888
#define MIB_INTERVAL_MS 80
#define SFN_MODULO 1024

int main() {
    int sockfd;
    struct sockaddr_in ue_addr;
    socklen_t addr_len = sizeof(ue_addr);

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // gNodeB_sfn
    unsigned short gNodeB_sfn = 0;

    // gNodeB gửi đến địa chỉ UE
    ue_addr.sin_family = AF_INET;
    ue_addr.sin_port = htons(PORT);
    ue_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    while (1) {
        // Mỗi 10ms: tăng SFN
        usleep(10000);
        gNodeB_sfn = (gNodeB_sfn + 1) % SFN_MODULO;

        // In ra SFN hiện tại
        printf("[gNodeB] Current SFN: %d\n", gNodeB_sfn);

        // Mỗi 80ms gửi bản tin MIB
        static int counter = 0;
        counter += 10;
        if (counter >= MIB_INTERVAL_MS) {
            counter = 0;
            unsigned char buffer[3];
            buffer[0] = 0x01; // message_id = 1
            buffer[1] = (gNodeB_sfn >> 8) & 0xFF;
            buffer[2] = gNodeB_sfn & 0xFF;

            sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&ue_addr, addr_len);
            printf("[gNodeB] Sent MIB: SFN = %d\n", gNodeB_sfn);
        }
    }

    close(sockfd);
    return 0;
}