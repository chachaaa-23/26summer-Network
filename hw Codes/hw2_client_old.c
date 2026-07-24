#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define BUF_SIZE 1024
void error_handling(char* message);

typedef struct{
    unsigned int mode;      // mode==1, Pkt / mode==2, ACK / mode==3, FIN
    unsigned int seq;       //sequence number
    unsigned char msg[1024];
} pkt_t;

int main(int argc, char* argv[]){
    int sock;
    char message[BUF_SIZE];
    int str_len;
    socklen_t adr_sz;
    int tot_datasize, tot_time=0;       //for throughput
    time_t start, end;
    int sender_seq=-1;
    int socktype;

    struct timeval optVal={10, 0};      //timeout value 
    int optLen = sizeof(optVal);

    struct sockaddr_in serv_adr, from_adr;
    if(argc != 3){
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(sock ==-1)   
        error_handling("socket() error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family=AF_INET;
    serv_adr.sin_addr.s_addr=inet_addr(argv[1]);
    serv_adr.sin_port=htons(atoi(argv[2]));

    pkt_t* send_pkt = (pkt_t *)malloc(sizeof(pkt_t));
    pkt_t* recv_pkt = (pkt_t *)malloc(sizeof(pkt_t));


    while(1){
        //user message 입력
        fputs("\nEnter Filename(q to quit): ", stdout);
        fgets(message, sizeof(message), stdin);
        if(!strcmp(message, "q\n") || !strcmp(message, "!\n"))
            break;

        memset(send_pkt, 0, sizeof(pkt_t));
        memset(recv_pkt, 0, sizeof(pkt_t));

        //send 패킷 설정
        send_pkt->mode = 1;      // 1 == Pkt
        sender_seq++;
        send_pkt->seq = sender_seq;
        strcpy(send_pkt->msg, message);

        while(1){
            sendto(sock, send_pkt, sizeof(*send_pkt), 0, (struct sockaddr*)&serv_adr, sizeof(serv_adr));
            if(socktype = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &optVal, optLen) != -1){
                // printf(">>socktype: %d\n", socktype);
                break;
            } else printf("Timeout! resending... (socktype: %d)\n", socktype);
        }

        //receive 패킷 설정
        int read_cnt;
        adr_sz = sizeof(from_adr);
        FILE* fp = fopen("receive_hw2.dat", "wb");

        while(1){
            read_cnt = recvfrom(sock, recv_pkt, sizeof(*recv_pkt), 0, (struct sockaddr*)&from_adr, &adr_sz);
            if(read_cnt == 0) break;
            if(socktype = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &optVal, optLen) != -1){
                fwrite((void*)recv_pkt->msg, 1, read_cnt, fp);
                if(recv_pkt->mode == 3) break;
            } else printf("Timeout! resending... (socktype: %d)\n", socktype);
        }
    
        // while((read_cnt = recvfrom(sock, recv_pkt, sizeof(*recv_pkt), 0, (struct sockaddr*)&from_adr, &adr_sz)) != 0){            
        //     fwrite((void*)recv_pkt->msg, 1, read_cnt, fp);
        //     if(recv_pkt->mode == 3) break;
        // }

        puts("Received file data\n");
	    fclose(fp);
        close(sock);
        break;
    }
    free(send_pkt);
    free(recv_pkt);
    
    return 0;
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}