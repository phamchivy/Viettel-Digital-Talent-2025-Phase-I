// File: ue.c - Completed
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

#define UE_PORT 5000
#define SYNC_PORT 8888
#define MAX_UE_COUNT 10
#define BUFFER_SIZE 1024

// Biến toàn cục để kiểm soát việc dừng các thread
volatile int running = 1;

// Biến toàn cục cho SFN (đồng bộ với gNodeB)
volatile unsigned int current_sfn = 0;
volatile int sfn_synchronized = 0;
pthread_mutex_t sfn_mutex = PTHREAD_MUTEX_INITIALIZER;

// Cấu trúc để truyền dữ liệu vào thread
typedef struct {
    int ue_id;
    int port;
} UEData;

// Hàm xử lý SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    printf("\nReceived signal %d, shutting down UEs...\n", sig);
    running = 0;
}

// Thread để đồng bộ SFN từ gNodeB
void* sfn_synchronizer(void* arg) {
    int sync_sockfd;
    struct sockaddr_in sync_addr, sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    
    // Tạo socket UDP để nhận SFN từ gNodeB
    sync_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sync_sockfd < 0) {
        perror("SFN sync socket creation failed");
        return NULL;
    }
    
    // Thiết lập thông tin địa chỉ
    sync_addr.sin_family = AF_INET;
    sync_addr.sin_addr.s_addr = INADDR_ANY;
    sync_addr.sin_port = htons(SYNC_PORT);
    
    // Gán địa chỉ và cổng
    if (bind(sync_sockfd, (struct sockaddr*)&sync_addr, sizeof(sync_addr)) < 0) {
        perror("SFN sync bind failed");
        close(sync_sockfd);
        return NULL;
    }
    
    printf("[UE] SFN Synchronizer started, listening on port %d\n", SYNC_PORT);
    
    while (running) {
        int sync_message[3];
        ssize_t bytes_received = recvfrom(sync_sockfd, sync_message, sizeof(sync_message), 0,
                                        (struct sockaddr*)&sender_addr, &sender_len);
        
        if (bytes_received < 0) {
            if (running) {
                perror("SFN sync recvfrom failed");
            }
            continue;
        }
        
        // Kiểm tra magic number để đảm bảo đây là gói tin SFN hợp lệ
        if (sync_message[0] == 0xABCD) {
            // Cập nhật SFN với mutex để tránh race condition
            pthread_mutex_lock(&sfn_mutex);
            current_sfn = sync_message[1];
            sfn_synchronized = 1;
            pthread_mutex_unlock(&sfn_mutex);
            
            // In thông tin SFN định kỳ
            if (current_sfn % 100 == 0) {
                printf("[UE] Synchronized with gNodeB SFN: %u, Subframe: %d\n", 
                       current_sfn, sync_message[2]);
            }
        }
    }
    
    close(sync_sockfd);
    printf("[UE] SFN Synchronizer terminated\n");
    return NULL;
}

// Hàm xử lý UE
void* handle_ue(void* arg) {
    UEData* data = (UEData*)arg;
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Tạo socket UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return NULL;
    }
    
    // Thiết lập thông tin địa chỉ
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(data->port);
    
    // Gán địa chỉ IP và PORT
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return NULL;
    }
    
    printf("UE %d started on port %d\n", data->ue_id, data->port);
    
    int message_count = 0;
    int volte_count = 0, data_count = 0;
    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);
    
    while (running) {
        // Mảng lớn hơn để nhận RRC Paging có chứa SFN (6 phần tử thay vì 4)
        int message[6];
        ssize_t bytes_received = recvfrom(sockfd, message, sizeof(message), 0,
                               (struct sockaddr*)&client_addr, &client_len);
        
        if (bytes_received < 0) {
            if (running) {
                perror("recvfrom failed");
            }
            continue;
        }
        
        message_count++;
        
        // Lấy thông tin từ bản tin
        int message_type = message[0];
        int ue_id = message[1];
        int message_id = message[2];
        int cn_domain = message[3];
        unsigned int gnb_sfn = message[4];
        int gnb_subframe = message[5];
        
        // Kiểm tra đồng bộ SFN
        pthread_mutex_lock(&sfn_mutex);
        int is_synchronized = sfn_synchronized;
        unsigned int ue_sfn = current_sfn;
        pthread_mutex_unlock(&sfn_mutex);
        
        // Phân loại bản tin theo cn_domain
        if (cn_domain == 100) { // VoLTE
            volte_count++;
        } else if (cn_domain == 101) { // Data
            data_count++;
        }
        
        // Kiểm tra SFN
        if (is_synchronized) {
            int sfn_diff = (gnb_sfn > ue_sfn) ? 
                          (gnb_sfn - ue_sfn) : (ue_sfn - gnb_sfn);
            
            // Nếu chênh lệch SFN > 1, đánh dấu là lỗi đồng bộ
            if (sfn_diff > 1 && message_count % 100 == 0) {
                printf("[UE %d] WARNING: SFN desynchronization detected! gNodeB SFN: %u, UE SFN: %u, Diff: %d\n",
                       data->ue_id, gnb_sfn, ue_sfn, sfn_diff);
            }
        }
        
        // In thống kê mỗi giây
        gettimeofday(&current_time, NULL);
        long elapsed_us = (current_time.tv_sec - start_time.tv_sec) * 1000000 + 
                         (current_time.tv_usec - start_time.tv_usec);
        
        if (elapsed_us >= 1000000) {
            if (message_count > 0) {
                int volte_percent = (volte_count * 100) / message_count;
                int data_percent = (data_count * 100) / message_count;
                
                printf("[UE %d] Received %d paging messages in %.2f seconds (cn_domain VoLTE: %d%%, Data: %d%%), SFN: %u\n", 
                       data->ue_id, message_count, elapsed_us / 1000000.0, 
                       volte_percent, data_percent, ue_sfn);
            }
            message_count = 0;
            volte_count = 0;
            data_count = 0;
            start_time = current_time;
        }
    }
    
    close(sockfd);
    printf("UE %d terminated\n", data->ue_id);
    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_t threads[MAX_UE_COUNT];
    pthread_t sfn_thread;
    UEData ue_data[MAX_UE_COUNT];
    int ue_count = 5; // Mặc định 5 UE
    
    // Khởi tạo mutex
    pthread_mutex_init(&sfn_mutex, NULL);
    
    // Xử lý tham số dòng lệnh
    if (argc > 1) {
        ue_count = atoi(argv[1]);
        if (ue_count <= 0 || ue_count > MAX_UE_COUNT) {
            ue_count = MAX_UE_COUNT;
        }
    }
    
    // Đăng ký handler xử lý tín hiệu Ctrl+C
    signal(SIGINT, handle_sigint);
    
    printf("Starting %d UEs with SFN synchronization\n", ue_count);
    
    // Tạo thread đồng bộ SFN
    if (pthread_create(&sfn_thread, NULL, sfn_synchronizer, NULL) != 0) {
        perror("Failed to create SFN synchronizer thread");
        return 1;
    }
    
    // Tạo các thread xử lý UE
    for (int i = 0; i < ue_count; i++) {
        ue_data[i].ue_id = i + 1;
        ue_data[i].port = UE_PORT + i;
        
        if (pthread_create(&threads[i], NULL, handle_ue, &ue_data[i]) != 0) {
            perror("Failed to create thread");
            return 1;
        }
    }
    
    // Chờ thread SFN
    pthread_join(sfn_thread, NULL);
    
    // Chờ các thread UE kết thúc
    for (int i = 0; i < ue_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Dọn dẹp
    pthread_mutex_destroy(&sfn_mutex);
    printf("All UEs shut down\n");
    return 0;
}