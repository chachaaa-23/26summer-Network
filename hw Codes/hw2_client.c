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
    unsigned int msg_read_cnt;
    unsigned char msg[1024];

} pkt_t;

int main(int argc, char* argv[]){
    int sock;
    char message[BUF_SIZE];
    socklen_t adr_sz;
    int tot_datasize=0;       //for throughput
    double tot_time=0;
    int sender_seq=-1;

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

    clock_t start = clock();
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
        sendto(sock, send_pkt, sizeof(*send_pkt), 0, (struct sockaddr*)&serv_adr, sizeof(serv_adr));

        //receive 패킷 설정
        int read_cnt;
        adr_sz = sizeof(from_adr);
        message[strlen(message)-1]='\0';
        FILE* fp = fopen(message, "wb");
        memset(send_pkt->msg, 0, sizeof(send_pkt->msg));
        int pass = 0;

        while(1){
            //Pkt 읽어오기,
            printf(">>start recvfrom... (pass: %d)\n", pass);
            read_cnt = recvfrom(sock, recv_pkt, sizeof(*recv_pkt), 0, (struct sockaddr*)&from_adr, &adr_sz);
            //^ read_cnt, 파일데이터 뿐만아니라 int 까지 함께 읽음. (read_cnt != 파일데이터 길이)

            if(read_cnt == 0) break;
            if(recv_pkt->seq == 2 && pass == 0) {
                printf(">>No ACK signal, sleep 6 sec ...\n");
                sleep(6);
                pass = 1;
                continue;
            }

            //Pkt 패킷 받은 경우 
            send_pkt->mode=2;   //ACK 패킷 보내기 
            send_pkt->seq = recv_pkt->seq;
            sendto(sock, send_pkt, sizeof(*send_pkt), 0, (struct sockaddr*)&serv_adr, sizeof(serv_adr));
            
            fwrite((void*)recv_pkt->msg, sizeof(char), recv_pkt->msg_read_cnt, fp);
            printf("ACK packet good\n");

            tot_datasize += recv_pkt->msg_read_cnt;
            if(recv_pkt->mode == 3) break;
        }

        puts("Received file data\n");
	    fclose(fp);
        close(sock);
        break;
    }
    clock_t end = clock();
    tot_time = ((double)(end-start) / CLOCKS_PER_SEC);
    printf("total time, total datasize: %f, %d\n", tot_time, tot_datasize);
    printf("throughput: %f Bps (Byte per Second)\n", ((double)tot_datasize/tot_time));

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