// File: gnodeb.c - Completed 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>

#define SERVER_PORT 6000
#define UE_PORT 5000
#define SYNC_PORT 8888
#define MAX_CONNECTIONS 100
#define BUFFER_SIZE 1024
#define MAX_QUEUE_SIZE 1000

// Biến toàn cục để kiểm soát việc dừng các thread
volatile int running = 1;

// Biến toàn cục để kiểm soát việc đồng bộ
volatile int is_sync = 0;

// Biến toàn cục cho SFN (System Frame Number)
volatile unsigned int current_sfn = 0;
pthread_mutex_t sfn_mutex = PTHREAD_MUTEX_INITIALIZER;

// Cấu trúc bản tin cho hàng đợi
typedef struct {
    int message[6];  // [message_type, ue_id, message_id, cn_domain, data1, data2]
    struct sockaddr_in ue_addr;
} QueueMessage;

// Cấu trúc hàng đợi
typedef struct {
    QueueMessage* messages;
    int front;
    int rear;
    int size;
    int capacity;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} MessageQueue;

// Khởi tạo hàng đợi toàn cục
MessageQueue message_queue;

// Cấu trúc để lưu thông tin kết nối
typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} ConnectionInfo;

// Hàm xử lý SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    printf("\nReceived signal %d, shutting down gNodeB...\n", sig);
    running = 0;
}

// Khởi tạo hàng đợi
void init_queue(MessageQueue* queue, int capacity) {
    queue->messages = (QueueMessage*)malloc(capacity * sizeof(QueueMessage));
    queue->front = 0;
    queue->rear = -1;
    queue->size = 0;
    queue->capacity = capacity;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
}

// Thêm bản tin vào hàng đợi
int enqueue(MessageQueue* queue, QueueMessage* message) {
    pthread_mutex_lock(&queue->mutex);
    
    // Đợi nếu hàng đợi đầy
    while (queue->size == queue->capacity && running) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }
    
    // Kiểm tra nếu thread đã dừng
    if (!running) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    
    // Thêm bản tin vào hàng đợi
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->messages[queue->rear] = *message;
    queue->size++;
    
    // Thông báo có bản tin mới
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

// Lấy bản tin từ hàng đợi
int dequeue(MessageQueue* queue, QueueMessage* message) {
    pthread_mutex_lock(&queue->mutex);
    
    // Đợi nếu hàng đợi trống
    while (queue->size == 0 && running) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    
    // Kiểm tra nếu thread đã dừng
    if (!running) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    
    // Lấy bản tin từ hàng đợi
    *message = queue->messages[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;
    
    // Thông báo có chỗ trống trong hàng đợi
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

// Hủy hàng đợi
void destroy_queue(MessageQueue* queue) {
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    free(queue->messages);
}

// Thread để quản lý và phát SFN
void* sfn_manager(void* arg) {
    int sync_sockfd;
    struct sockaddr_in sync_addr;
    
    // Tạo socket UDP cho đồng bộ SFN
    sync_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sync_sockfd < 0) {
        perror("SFN sync socket creation failed");
        return NULL;
    }
    
    // Thiết lập thông tin địa chỉ
    sync_addr.sin_family = AF_INET;
    sync_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    sync_addr.sin_port = htons(SYNC_PORT);
    
    printf("[gNodeB] SFN Manager started, broadcasting on port %d\n", SYNC_PORT);
    
    // Biến để theo dõi SFN và thời gian
    struct timeval last_update, now;
    gettimeofday(&last_update, NULL);
    
    while (running) {
        gettimeofday(&now, NULL);
        long elapsed_ms = ((now.tv_sec - last_update.tv_sec) * 1000) +
                          ((now.tv_usec - last_update.tv_usec) / 1000);
        
        // SFN tăng mỗi 10ms (độ dài của 1 radio frame)
        if (elapsed_ms >= 10) {
            // Cập nhật SFN với mutex để tránh race condition
            pthread_mutex_lock(&sfn_mutex);
            current_sfn = (current_sfn + 1) % 1024; // SFN có 10 bit (0-1023)
            unsigned int sfn_to_send = current_sfn;
            pthread_mutex_unlock(&sfn_mutex);
            
            // Cấu trúc bản tin: [magic_number(0xABCD), sfn, subframe(0-9)]
            int sync_message[3] = {0xABCD, sfn_to_send, (elapsed_ms / 10) % 10};
            
            // Broadcast SFN tới tất cả UE
            sendto(sync_sockfd, sync_message, sizeof(sync_message), 0,
                  (struct sockaddr*)&sync_addr, sizeof(sync_addr));
            
            // Kích hoạt việc đồng bộ sau SFN 10
            if (sfn_to_send > 10 && !is_sync) {
                is_sync = 1;
                printf("[gNodeB] System synchronized, starting to process messages\n");
            }
            
            // Cập nhật lại thời gian
            last_update = now;
            
            // In thông tin SFN định kỳ
            if (sfn_to_send % 100 == 0) {
                printf("[gNodeB] Current SFN: %u\n", sfn_to_send);
            }
        }
        
        // Sleep một khoảng ngắn để giảm tải CPU
        usleep(1000); // Sleep 1ms
    }
    
    close(sync_sockfd);
    printf("[gNodeB] SFN Manager terminated\n");
    return NULL;
}

// Thread để gửi các bản tin RRC
void* rrc_sender(void* arg) {
    // Socket để gửi tin nhắn tới UE
    int ue_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ue_sockfd < 0) {
        perror("UE socket creation failed");
        return NULL;
    }
    
    printf("[gNodeB] RRC Sender started\n");
    
    int message_count = 0;
    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);
    
    while (running) {
        // Chỉ xử lý bản tin khi đã đồng bộ
        if (is_sync) {
            // Lấy bản tin từ hàng đợi
            QueueMessage message;
            if (dequeue(&message_queue, &message) == 0) {
                // Lấy SFN hiện tại với mutex
                pthread_mutex_lock(&sfn_mutex);
                unsigned int current_sfn_value = current_sfn;
                pthread_mutex_unlock(&sfn_mutex);
                
                // Thêm thông tin SFN vào bản tin
                message.message[4] = current_sfn_value;
                message.message[5] = (current_sfn_value % 10);
                
                // Gửi bản tin tới UE
                sendto(ue_sockfd, message.message, sizeof(message.message), 0,
                      (struct sockaddr*)&message.ue_addr, sizeof(message.ue_addr));
                
                message_count++;
                
                // In thống kê mỗi giây
                gettimeofday(&current_time, NULL);
                long elapsed_us = (current_time.tv_sec - start_time.tv_sec) * 1000000 +
                                  (current_time.tv_usec - start_time.tv_usec);
                
                if (elapsed_us >= 1000000) {
                    printf("[gNodeB] Processed %d RRC messages in %.2f seconds (Current SFN: %u)\n",
                           message_count, elapsed_us / 1000000.0, current_sfn_value);
                    message_count = 0;
                    start_time = current_time;
                }
            }
        } else {
            // Chờ cho đến khi hệ thống đồng bộ
            usleep(10000); // Sleep 10ms
        }
    }
    
    close(ue_sockfd);
    printf("[gNodeB] RRC Sender terminated\n");
    return NULL;
}

// Hàm xử lý kết nối từ AMF
void* handle_amf_connection(void* arg) {
    ConnectionInfo* conn_info = (ConnectionInfo*)arg;
    int client_socket = conn_info->client_socket;
    struct sockaddr_in client_addr = conn_info->client_addr;
    free(conn_info); // Giải phóng bộ nhớ đã cấp phát
    
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    printf("AMF connected from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
    
    int message_count = 0;
    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);
    
    while (running) {
        int message[4];
        ssize_t bytes_received = recv(client_socket, message, sizeof(message), 0);
        
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("AMF disconnected\n");
            } else {
                perror("recv failed");
            }
            break;
        }
        
        message_count++;
        
        // Lấy thông tin từ bản tin
        int message_type = message[0];
        int ue_id = message[1];
        int message_id = message[2];
        int cn_domain = message[3];
        
        // Tạo bản tin cho hàng đợi
        QueueMessage queue_message;
        queue_message.message[0] = message_type;
        queue_message.message[1] = ue_id;
        queue_message.message[2] = message_id;
        queue_message.message[3] = cn_domain;
        queue_message.message[4] = 0; // SFN sẽ được cập nhật bởi rrc_sender
        queue_message.message[5] = 0; // Subframe sẽ được cập nhật bởi rrc_sender
        
        // Thiết lập địa chỉ UE
        queue_message.ue_addr.sin_family = AF_INET;
        queue_message.ue_addr.sin_port = htons(UE_PORT + (ue_id % 1000)); // Phân bố UE trên các cổng khác nhau
        queue_message.ue_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        // Thêm bản tin vào hàng đợi
        if (enqueue(&message_queue, &queue_message) < 0) {
            printf("[gNodeB] Failed to enqueue message, queue might be full or system shutting down\n");
        }
        
        // In thống kê mỗi giây
        gettimeofday(&current_time, NULL);
        long elapsed_us = (current_time.tv_sec - start_time.tv_sec) * 1000000 +
                          (current_time.tv_usec - start_time.tv_usec);
        
        if (elapsed_us >= 1000000) {
            printf("[gNodeB] Received %d NGAP messages in %.2f seconds\n",
                   message_count, elapsed_us / 1000000.0);
            message_count = 0;
            start_time = current_time;
        }
    }
    
    close(client_socket);
    printf("AMF connection handler terminated\n");
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    
    // Đăng ký handler xử lý tín hiệu Ctrl+C
    signal(SIGINT, handle_sigint);
    
    // Khởi tạo mutex
    pthread_mutex_init(&sfn_mutex, NULL);
    
    // Khởi tạo hàng đợi bản tin
    init_queue(&message_queue, MAX_QUEUE_SIZE);
    
    // Tạo thread quản lý SFN
    pthread_t sfn_thread;
    if (pthread_create(&sfn_thread, NULL, sfn_manager, NULL) != 0) {
        perror("Failed to create SFN manager thread");
        return 1;
    }
    
    // Tạo thread xử lý và gửi bản tin RRC
    pthread_t rrc_thread;
    if (pthread_create(&rrc_thread, NULL, rrc_sender, NULL) != 0) {
        perror("Failed to create RRC sender thread");
        return 1;
    }
    
    // Tạo socket file descriptor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    // Thiết lập socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        close(server_fd);
        return 1;
    }
    
    // Thiết lập thông tin địa chỉ
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);
    
    // Gán địa chỉ IP và PORT
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return 1;
    }
    
    // Lắng nghe kết nối đến
    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("Listen failed");
        close(server_fd);
        return 1;
    }
    
    printf("gNodeB started on port %d, waiting for AMF connections...\n", SERVER_PORT);
    
    pthread_t thread_id;
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        // Chấp nhận kết nối từ AMF
        int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            if (running) {
                perror("Accept failed");
            }
            continue;
        }
        
        // Cấp phát và khởi tạo cấu trúc thông tin kết nối
        ConnectionInfo* conn_info = malloc(sizeof(ConnectionInfo));
        if (!conn_info) {
            perror("Memory allocation failed");
            close(client_socket);
            continue;
        }
        
        conn_info->client_socket = client_socket;
        conn_info->client_addr = client_addr;
        
        // Tạo thread mới để xử lý kết nối
        if (pthread_create(&thread_id, NULL, handle_amf_connection, conn_info) != 0) {
            perror("Failed to create thread");
            free(conn_info);
            close(client_socket);
        }
        
        // Tách thread để nó tự chạy độc lập
        pthread_detach(thread_id);
    }
    
    // Đợi thread kết thúc
    pthread_join(sfn_thread, NULL);
    pthread_join(rrc_thread, NULL);
    
    // Dọn dẹp
    close(server_fd);
    pthread_mutex_destroy(&sfn_mutex);
    destroy_queue(&message_queue);
    
    printf("gNodeB shutdown complete\n");
    return 0;
}