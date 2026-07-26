#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>

#define BUF_SIZE 1024
void error_handling(char* message);

typedef struct{
    unsigned int mode;      // mode==1, Pkt / mode==2, ACK
    unsigned int seq;       //sequence number
    unsigned int msg_read_cnt;
    unsigned char msg[1024];

} pkt_t;

int main(int argc, char* argv[]){
    int serv_sock;
    char message[BUF_SIZE];
    socklen_t clnt_adr_sz;
    char filename[100];
    int seq=0;
        int tot_datasize=0;       //for throughput

    int socktype;
    struct timeval optVal={3, 0};      //timeout value 
    int optLen = sizeof(optVal);

    struct sockaddr_in serv_adr, clnt_adr;
    if(argc !=2){
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(serv_sock == -1)
        error_handling("UDP socker creation error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)))
        error_handling("bind() error");

    pkt_t* send_pkt = (pkt_t *)malloc(sizeof(pkt_t));
    pkt_t* recv_pkt = (pkt_t *)malloc(sizeof(pkt_t));
    
    memset(message, 0, sizeof(message));
    clnt_adr_sz = sizeof(clnt_adr);

    // 1. filename 받기
    recvfrom(serv_sock, recv_pkt, sizeof(*recv_pkt), 0, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
    if(recv_pkt->mode == 1){    //패킷신호 == Pkt
        strcpy(filename, recv_pkt->msg);
    } else if(recv_pkt->mode == 2){
        printf("<ACK> Wrong packet mode. \n");
    } else printf("Packet mode error");
    //2. file 검색 후 내용 전송
    /* 2-1. 현재 디렉토리의 모든 파일 목록, 알아내서, 문자열에 저장*/
    DIR* dir = opendir("./");
    if(dir==NULL){
        perror("failed to open dir\n");
        return 0;
    }
    struct dirent* entry;
    while((entry=readdir(dir)) != NULL){
        if(entry->d_type == DT_REG){
            strcat(message, entry->d_name);
            strcat(message, "\n");              // 파일 구분
        }
    }

    // 2-2. read filename and get
    int read_cnt;   
    if(strstr(message, filename) != NULL){      //find if file exists // 2-3. 해당 파일 내용, 전송. 
        filename[strlen(filename)-1] = '\0';    //newline 삭제
        FILE* fp = fopen(filename, "rb");

        int pass=1;
        while(1){
            printf(">pass: %d\n", pass);
            if(pass == 1){
                memset(message, 0, sizeof(message));
                read_cnt = fread(message, 1, BUF_SIZE, fp);   //현재 파일 포인터부터 500씩 읽어서 msg에 저장해라. 몇개의 항목을 읽었는지 read_cnt에 저장해라.
                // strcpy(send_pkt->msg, message);
                memcpy(send_pkt->msg, message, read_cnt);
                
                send_pkt->msg_read_cnt = read_cnt;
                seq++;
                send_pkt->seq = seq;
            }

            if(read_cnt < BUF_SIZE){
                send_pkt->mode = 3;     // final send mode!
                memset(send_pkt->msg + (int)read_cnt, 0, sizeof(send_pkt->msg) - read_cnt);
            } else {
                send_pkt->mode = 1;         // send pkt 설정
            }

            printf(">>send_pkt->mode: %d\n", send_pkt->mode);
            printf(">>send_pkt->seq: %d\n", send_pkt->seq);
            // printf(">>send_pkt->msg: %s\n", (send_pkt->msg));

            sendto(serv_sock, send_pkt, sizeof(*send_pkt), 0, (struct sockaddr*)&clnt_adr, clnt_adr_sz);    //send file data
            socktype = recvfrom(serv_sock, recv_pkt, sizeof(*recv_pkt), 0, (struct sockaddr*)&clnt_adr, &clnt_adr_sz); //receive ACK.

            setsockopt(serv_sock, SOL_SOCKET, SO_RCVTIMEO, &optVal, optLen);
            printf("socktype: %d\n", socktype);
            
            if (socktype != -1){       //not timeout 시
                printf("ACK received. (socktype: %d)\n", socktype);
                pass = 1;
                tot_datasize += send_pkt->msg_read_cnt;
                if(send_pkt->mode == 3) break;
            } else {        //패킷 손실, timeout 시 다시 메시지 전송
                printf("Timeout! send again... (socktype: %d)\n", socktype);
                pass = 0;                                 
                continue;
            }
        }

        printf("file 전송 완료. fclose.\n\n");

    printf("total datasize: %d\n", tot_datasize);

        fclose(fp);
    } else {
        perror("File name incorrect. Enter again. \n\n");
    }
    
    
    free(send_pkt);
    free(recv_pkt);
    close(serv_sock);
    return 0;
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}