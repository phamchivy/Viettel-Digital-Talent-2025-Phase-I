// File: gNodeB.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define UDP_PORT 5000
#define UDP_PORT_SYNC 8888
#define TCP_PORT 6000
#define UE_IP "127.0.0.1"

#define MAX_QUEUE_SIZE 100

typedef struct {
    int paging_msg[4];
} PagingMessage;

// Paging queue
PagingMessage paging_queue[MAX_QUEUE_SIZE];
int queue_front = 0;
int queue_rear = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
int has_sync = 0;

void enqueue_paging(PagingMessage msg) {
    pthread_mutex_lock(&queue_mutex);
    if ((queue_rear + 1) % MAX_QUEUE_SIZE != queue_front) {
        paging_queue[queue_rear] = msg;
        queue_rear = (queue_rear + 1) % MAX_QUEUE_SIZE;
    } else {
        printf("[gNodeB] Paging queue full! Dropping message.\n");
    }
    pthread_mutex_unlock(&queue_mutex);
}

int dequeue_paging(PagingMessage* msg) {
    pthread_mutex_lock(&queue_mutex);
    if (queue_front != queue_rear) {
        *msg = paging_queue[queue_front];
        queue_front = (queue_front + 1) % MAX_QUEUE_SIZE;
        pthread_mutex_unlock(&queue_mutex);
        return 1; // Success
    }
    pthread_mutex_unlock(&queue_mutex);
    return 0; // Empty
}

void* handle_client(void* client_socket) {
    int client_fd = *((int*)client_socket);
    free(client_socket);
    
    while (1) {
        PagingMessage new_msg;
        int n = recv(client_fd, new_msg.paging_msg, sizeof(new_msg.paging_msg), 0);
        
        if (n <= 0) {
            // Connection closed or error
            break;
        }
        
        printf("[gNodeB] Received NgAP Paging: ue_id = %d, cn_domain = %d\n",
               new_msg.paging_msg[1], new_msg.paging_msg[3]);
        enqueue_paging(new_msg);
    }
    
    printf("[gNodeB] Client disconnected\n");
    close(client_fd);
    return NULL;
}

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

    printf("[gNodeB] TCP server started on port %d\n", TCP_PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("[gNodeB] Accept failed");
            continue;
        }
        
        printf("[gNodeB] New client connected: %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        // Create a new thread for each client
        pthread_t client_thread;
        int* client_sock = malloc(sizeof(int));
        *client_sock = client_fd;
        
        if (pthread_create(&client_thread, NULL, handle_client, (void*)client_sock) != 0) {
            perror("[gNodeB] Failed to create client thread");
            close(client_fd);
            free(client_sock);
        } else {
            // Detach thread so it can clean up itself
            pthread_detach(client_thread);
        }
    }
    
    close(server_fd);
    return NULL;
}

void* paging_rrc_thread(void* arg) {
    int sockfd;
    struct sockaddr_in ue_addr;
    socklen_t addr_len = sizeof(ue_addr);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    ue_addr.sin_family = AF_INET;
    ue_addr.sin_port = htons(UDP_PORT);
    ue_addr.sin_addr.s_addr = inet_addr(UE_IP);

    while (1) {
        usleep(5000); // kiểm tra mỗi 5ms

        if (has_sync) {
            PagingMessage msg;
            if (dequeue_paging(&msg)) {
                unsigned char buffer[3];
                buffer[0] = 0x64; // message_id = 2: Paging RRC
                buffer[1] = msg.paging_msg[1]; // ue_id
                buffer[2] = msg.paging_msg[3]; // cn_domain

                sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&ue_addr, addr_len);
                printf("[gNodeB] Sent Paging RRC: ue_id = %d, cn_domain = %d\n", msg.paging_msg[1], msg.paging_msg[3]);
            }
        }
    }

    close(sockfd);
    return NULL;
}

void* sfn_thread(void* arg) {
    int sockfd;
    struct sockaddr_in ue_addr;
    socklen_t addr_len = sizeof(ue_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    unsigned short gNodeB_sfn = 0;

    ue_addr.sin_family = AF_INET;
    ue_addr.sin_port = htons(UDP_PORT_SYNC);
    ue_addr.sin_addr.s_addr = inet_addr(UE_IP);

    while (1) {
        usleep(10000); // mỗi 10ms
        gNodeB_sfn = (gNodeB_sfn + 1) % 1024;

        //printf("[gNodeB] Current SFN: %d\n", gNodeB_sfn);
        has_sync = 0;

        static int counter = 0;
        counter += 10;
        if (counter >= 80) {
            counter = 0;
            unsigned char buffer[3];
            buffer[0] = 0x01; // message_id = 1 (MIB)
            buffer[1] = (gNodeB_sfn >> 8) & 0xFF;
            buffer[2] = gNodeB_sfn & 0xFF;

            sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&ue_addr, addr_len);
            printf("[gNodeB] Sent MIB: SFN = %d\n", gNodeB_sfn);
            has_sync = 1;
        }
    }

    close(sockfd);
    return NULL;
}

void* print_queue_thread(void* arg) {
    while (1) {
        sleep(1);  // mỗi giây in 1 lần

        pthread_mutex_lock(&queue_mutex);
        printf("[gNodeB] Paging Queue (front=%d, rear=%d): ", queue_front, queue_rear);
        if (queue_front == queue_rear) {
            printf("EMPTY\n");
        } else {
            int i = queue_front;
            while (i != queue_rear) {
                int* msg = paging_queue[i].paging_msg;
                printf("[ue_id=%d, cn_domain=%d] ", msg[1], msg[3]);
                i = (i + 1) % MAX_QUEUE_SIZE;
            }
            printf("\n");
        }
        pthread_mutex_unlock(&queue_mutex);
    }
    return NULL;
}

int main() {
    pthread_t tcp_thread, sfn_updater, paging_thread;

    pthread_create(&tcp_thread, NULL, tcp_server_thread, NULL);
    pthread_create(&sfn_updater, NULL, sfn_thread, NULL);
    pthread_create(&paging_thread, NULL, paging_rrc_thread, NULL);
    //pthread_create(&queue_printer, NULL, print_queue_thread, NULL);

    pthread_join(tcp_thread, NULL);
    pthread_join(sfn_updater, NULL);
    pthread_join(paging_thread, NULL);
    //pthread_join(queue_printer,NULL);

    return 0;
}