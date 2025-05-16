#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 6000
#define NUM_THREADS 5
#define MESSAGES_PER_SECOND 500
#define MAX_UE_COUNT 10000

// Biến toàn cục để kiểm soát việc dừng các thread
volatile int running = 1;

// Cấu trúc để truyền dữ liệu vào thread
typedef struct {
    int thread_id;
    int start_ue_id;
    int end_ue_id;
} ThreadData;

// Hàm xử lý SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    running = 0;
}

// Hàm gửi các bản tin NGAP của thread
void* send_ngap_messages(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    int sockfd;
    struct sockaddr_in server_addr;
    
    // Tạo socket TCP
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return NULL;
    }
    
    // Thiết lập thông tin địa chỉ gNodeB
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    
    // Kết nối tới gNodeB
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to gNodeB failed");
        close(sockfd);
        return NULL;
    }
    
    printf("Thread %d connected to gNodeB, handling UE IDs from %d to %d\n", 
           data->thread_id, data->start_ue_id, data->end_ue_id);
    
    int messages_per_thread = MESSAGES_PER_SECOND / NUM_THREADS;
    int ue_range = data->end_ue_id - data->start_ue_id + 1;
    int current_ue_index = 0;
    
    struct timeval now, prev;
    gettimeofday(&prev, NULL);
    
    int message_count = 0;
    
    // Vòng lặp chính gửi bản tin
    while (running) {
        // Tính UE ID hiện tại
        int ue_id = data->start_ue_id + (current_ue_index % ue_range);
        current_ue_index++;
        
        // Luân phiên giữa VoLTE và Data
        int cn_domain = (ue_id % 2 == 0) ? 100 : 101;
        
        // Chuẩn bị bản tin NGAP: message_type, ue_id, message_id, cn_domain
        int message[4] = {100, ue_id, 100, cn_domain};
        
        ssize_t sent_bytes = send(sockfd, message, sizeof(message), 0);
        if (sent_bytes < 0) {
            perror("Send failed");
            break;
        }
        
        message_count++;
        
        // Kiểm tra tốc độ gửi và điều chỉnh nếu cần
        gettimeofday(&now, NULL);
        long elapsed_us = (now.tv_sec - prev.tv_sec) * 1000000 + (now.tv_usec - prev.tv_usec);
        
        // Nếu đã qua 1 giây, in thống kê và reset đếm
        if (elapsed_us >= 1000000) {
            printf("Thread %d: Sent %d messages in %.2f seconds\n", 
                   data->thread_id, message_count, elapsed_us / 1000000.0);
            message_count = 0;
            gettimeofday(&prev, NULL);
        }
        
        // Tính toán thời gian sleep để đạt được tốc độ mong muốn
        long sleep_us = (1000000 / messages_per_thread) - (elapsed_us % (1000000 / messages_per_thread));
        if (sleep_us > 0 && sleep_us < 1000000) {
            usleep(sleep_us);
        }
    }
    
    // Đóng socket khi thoát khỏi vòng lặp
    close(sockfd);
    printf("Thread %d terminated\n", data->thread_id);
    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_t threads[NUM_THREADS];
    ThreadData thread_data[NUM_THREADS];
    int total_ue_count = MAX_UE_COUNT;
    
    // Xử lý tham số dòng lệnh
    if (argc > 1) {
        total_ue_count = atoi(argv[1]);
        if (total_ue_count <= 0) {
            total_ue_count = MAX_UE_COUNT;
        }
    }
    
    printf("AMF starting with %d UEs distributed across %d threads\n", 
           total_ue_count, NUM_THREADS);

    // Đăng ký handler xử lý tín hiệu Ctrl+C
    signal(SIGINT, handle_sigint);
    
    // Khởi tạo và chạy các thread
    int ues_per_thread = total_ue_count / NUM_THREADS;
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].start_ue_id = i * ues_per_thread + 1;
        thread_data[i].end_ue_id = (i == NUM_THREADS - 1) ? 
                                  total_ue_count : 
                                  (i + 1) * ues_per_thread;
        
        if (pthread_create(&threads[i], NULL, send_ngap_messages, &thread_data[i]) != 0) {
            perror("Failed to create thread");
            return 1;
        }
    }
    
    // Chờ các thread kết thúc
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("AMF shutdown complete\n");
    return 0;
}