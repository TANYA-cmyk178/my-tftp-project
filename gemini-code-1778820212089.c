#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define TFTP_PORT 69
#define BUF_SIZE 1024
#define DATA_SIZE 512

// TFTP Opcode
#define OP_RRQ   1
#define OP_DATA  3
#define OP_ACK   4
#define OP_ERROR 5

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <Server IP> <Filename>\n", argv[0]);
        exit(1);
    }

    int sock;
    struct sockaddr_in serv_addr, from_addr;
    socklen_t from_len;
    char buffer[BUF_SIZE];
    int len, block_num = 0;

    // 1. UDP 소켓 생성
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(TFTP_PORT); // 첫 목적지는 69번 포트

    // 2. RRQ 패킷 생성 (Opcode 2bytes + Filename + 0 + Mode + 0)
    int req_len = sprintf(buffer, "%c%c%s%c%s%c", 0x00, OP_RRQ, argv[2], 0x00, "octet", 0x00);

    // 3. 서버의 69번 포트로 요청 전송
    sendto(sock, buffer, req_len, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    printf(">> Read Request 전송: %s (Port: 69)\n", argv[2]);

    while (1) {
        from_len = sizeof(from_addr);
        // 4. 데이터 수신 (중요: 여기서 서버의 '임시 포트' 주소가 from_addr에 저장됨)
        len = recvfrom(sock, buffer, BUF_SIZE, 0, (struct sockaddr*)&from_addr, &from_len);

        int opcode = ntohs(*(short *)buffer);
        if (opcode == OP_DATA) {
            block_num = ntohs(*(short *)(buffer + 2));
            printf("<< 데이터 수신: Block #%d, Size: %d bytes (From Port: %d)\n", 
                    block_num, len - 4, ntohs(from_addr.sin_port));

            // 5. ACK 패킷 생성 및 전송 (받은 포트번호로 다시 보냄)
            char ack[4];
            ack[0] = 0x00; ack[1] = OP_ACK;
            ack[2] = buffer[2]; ack[3] = buffer[3];
            
            sendto(sock, ack, 4, 0, (struct sockaddr*)&from_addr, from_len);
            printf(">> ACK 전송: Block #%d\n", block_num);

            // 6. 종료 조건 (데이터 크기가 512바이트 미만일 때)
            if (len - 4 < DATA_SIZE) {
                printf("### 전송 완료 (마지막 블록 확인) ###\n");
                break;
            }
        } else if (opcode == OP_ERROR) {
            printf("Error: 서버에서 오류 메시지를 보냈습니다.\n");
            break;
        }
    }

    close(sock);
    return 0;
}