#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define TFTP_PORT 69
#define HTTP_PORT 8080  // 웹 브라우저 접속 포트
#define BUF_SIZE 1024

char status_msg[BUF_SIZE] = "대기 중..."; // 브라우저에 표시될 상태 정보

// --- 웹 서버 스레드 (브라우저 모니터링용) ---
void* http_server_thread(void* arg) {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(HTTP_PORT);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    while(1) {
        client_fd = accept(server_fd, NULL, NULL);
        char response[BUF_SIZE * 2];
        // 브라우저에 현재 TFTP 전송 상태를 HTML로 전달
        sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
                          "<html><body><h1>TFTP 모니터링</h1><p>상태: %s</p>"
                          "<script>setTimeout(()=>location.reload(), 1000);</script></body></html>", 
                          status_msg);
        write(client_fd, response, strlen(response));
        close(client_fd);
    }
    return NULL;
}

// --- TFTP 클라이언트 메인 로직 ---
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("사용법: %s <서버IP> <파일명>\n", argv[0]);
        return 1;
    }

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, http_server_thread, NULL);
    printf("브라우저에서 http://localhost:%d 접속 시 모니터링 가능\n", HTTP_PORT);

    int sock;
    struct sockaddr_in serv_addr, from_addr;
    socklen_t from_len;
    char buffer[BUF_SIZE];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(TFTP_PORT);

    // Read Request 전송
    sprintf(status_msg, "파일 '%s' 요청 중...", argv[2]);
    int req_len = sprintf(buffer, "%c%c%s%c%s%c", 0x00, 1, argv[2], 0x00, "octet", 0x00);
    sendto(sock, buffer, req_len, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    while (1) {
        from_len = sizeof(from_addr);
        int len = recvfrom(sock, buffer, BUF_SIZE, 0, (struct sockaddr*)&from_addr, &from_len);
        
        int opcode = ntohs(*(short *)buffer);
        if (opcode == 3) { // DATA 패킷
            int block = ntohs(*(short *)(buffer + 2));
            sprintf(status_msg, "데이터 수신 중: 블록 #%d (%d bytes)", block, len - 4);
            printf("%s\n", status_msg);

            // ACK 전송
            char ack[4] = {0, 4, buffer[2], buffer[3]};
            sendto(sock, ack, 4, 0, (struct sockaddr*)&from_addr, from_len);

            if (len - 4 < 512) {
                strcpy(status_msg, "전송 완료!");
                printf("완료!\n");
                break;
            }
        }
    }

    sleep(5); // 결과 확인을 위해 잠시 대기
    close(sock);
    return 0;
}
