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

    // Tạo socket TCP
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Thiết lập thông tin địa chỉ gNodeB
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Kết nối tới gNodeB
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to gNodeB failed");
        close(sockfd);
        return 1;
    }

    while (1) {
        int cn_domain;
        printf("Enter cn_domain (100 = VoLTE, 101 = Data, -1 to exit): ");
        scanf("%d", &cn_domain);

        if (cn_domain == -1) {
            printf("Exiting...\n");
            break;
        }

        if (cn_domain != 100 && cn_domain != 101) {
            printf("Invalid cn_domain. Only 100 (VoLTE) or 101 (Data) allowed.\n");
            continue;
        }

        int message[4] = {100, 12345, 100, cn_domain};
        ssize_t sent_bytes = send(sockfd, message, sizeof(message), 0);
        if (sent_bytes < 0) {
            perror("Send failed");
            break;
        }

        printf("[AMF] Sent NGAP Paging message with cn_domain = %d\n", cn_domain);
    }

    // Đóng socket khi thoát khỏi vòng lặp
    close(sockfd);
    return 0;
}