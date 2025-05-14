// File: UE.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 5000
#define DRX_CYCLE_MS 160

int main() {
    int sockfd;
    struct sockaddr_in my_addr;
    socklen_t addr_len = sizeof(my_addr);
    
    int buf[4];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sockfd, (struct sockaddr*)&my_addr, sizeof(my_addr));

    while (1) {
        usleep(DRX_CYCLE_MS * 1000); // Giả lập chu kỳ DRX

        int n = recvfrom(sockfd, buf, sizeof(buf), MSG_DONTWAIT,
                         (struct sockaddr*)&my_addr, &addr_len);

        if (n > 0 && buf[0] == 100) {
            printf("[UE] Received RRC Paging: ue_id = %d, cn_domain = %d\n",
                   buf[1], buf[3]);
        } else {
            printf("[UE] Woke up, no paging\n");
        }
    }

    close(sockfd);
    return 0;
}