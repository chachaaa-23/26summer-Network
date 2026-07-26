#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 512
void error_handling(char *message);

typedef struct {
    int option;     //1-4
    int msg_read_cnt;
    int file_eof;   //파일의 끝 여부.  0==메시지 남음, 1==메시지 끝
    char msg[BUF_SIZE];
} pkt_t;

int main(int argc, char *argv[])
{
	int sd;
	FILE *fp;
	char buf[BUF_SIZE];
	int read_cnt;
	struct sockaddr_in serv_adr;
    char serv_path[50];     //서버의 dir path
	if (argc != 3) {
		printf("Usage: %s <IP> <port>\n", argv[0]);
		exit(1);
	}
	
	// fp = fopen("d3_receive.dat", "wb");
	sd = socket(PF_INET, SOCK_STREAM, 0);   
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_adr.sin_port = htons(atoi(argv[2]));
	if(connect(sd, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)     //서버로 연결 요청
		error_handling("connect() error!");
	else puts("Connected...........");

    pkt_t* send_pkt = (pkt_t*)malloc(sizeof(pkt_t));
    pkt_t* recv_pkt = (pkt_t*)malloc(sizeof(pkt_t));
    
    char option[1];
	while(1){
	    memset(buf, 0, sizeof(buf));  
        memset(send_pkt, 0, sizeof(send_pkt));
        printf("\n> Select Option ...\n");
        printf("    1: Get Current Directory's Infomation\n    2: Move Current Directory\n    3: Download Specific file\n    4: Upload Specific file\n");
        scanf("%s", option);
        fflush(stdout);
        printf(">>option: %s\n", option);

		if (!strcmp(option,"q") || !strcmp(option,"Q"))
			break;
        // printf(">>buf: %s\n", buf);  
        send_pkt->option = atoi(option);

        switch (send_pkt->option){
            case 2:     //이동할 절대경로 입력
                printf("\nEnter path to move ...\n");
                scanf("%s", send_pkt->msg);
                fflush(stdout);
                send_pkt->msg_read_cnt = strlen(send_pkt->msg);

                printf(">>send_pkt->msg ::%s::\n", send_pkt->msg);
                strcpy(serv_path, send_pkt->msg);
                
                break;
            case 3:     //다운받을 서버의 파일 입력
                printf("\nEnter filename to download ...\n");
                scanf("%s", send_pkt->msg);
                fflush(stdout);
                send_pkt->msg_read_cnt = strlen(send_pkt->msg);
                send_pkt->msg[send_pkt->msg_read_cnt-1]='\0';

                printf(">>send_pkt->msg: %s\n", send_pkt->msg);
                printf(">>send_pkt->msg_read_cnt: %d\n", send_pkt->msg_read_cnt);

                break;

        }
        
		write(sd, send_pkt, sizeof(*send_pkt));    //클라이언트 소켓의 출력 버퍼, 사용자 메시지 보내기 write
        printf("option sent... (op: %d)\n", send_pkt->option);
        printf(">>send_pkt->msg: %s\n", send_pkt->msg);
        printf(">>send_pkt->msg_read_cnt: %d\n", send_pkt->msg_read_cnt);
        printf(">>send_pkt->file_eof: %d\n", send_pkt->file_eof);


        memset(recv_pkt, 0, sizeof(recv_pkt));
        
        int total_cnt=0;
        while(total_cnt < sizeof(*recv_pkt)){
            read_cnt = read(sd, recv_pkt, sizeof(*recv_pkt));
            total_cnt += read_cnt;
        }
        // read(sd, recv_pkt, sizeof(*recv_pkt));

        printf("option received... (op: %d)\n\n", recv_pkt->option);
        printf(">>recv_pkt->msg_read_cnt: %d\n", recv_pkt->msg_read_cnt);
        printf(">>recv_pkt->msg: %s\n", recv_pkt->msg);
        printf(">>recv_pkt->file_eof: %d\n", recv_pkt->file_eof);


        switch (recv_pkt->option){
            case 1:   //서버의 현 디렉토리 정보출력
                //서버로부터 read 후 printf
                printf("\nCurrent Directory's Information: %.*s\n", recv_pkt->msg_read_cnt , recv_pkt->msg);
                break;

            case 2:   //서버의 현 디렉토리 이동
                printf("\n%s\n", recv_pkt->msg);
                break;

            case 3:   //서버의 파일 다운
                printf("\nDownloading file... (%s)\n ", send_pkt->msg);
                FILE* fp = fopen(send_pkt->msg, "wb");

                while(1){                //현재 서버로부터 받은 소켓, 읽어들이고 read
                    read_cnt = read(sd, recv_pkt, sizeof(*recv_pkt));
                    if(read_cnt == 0) break;

                	printf(">>recv_pkt->msg: %.*s\n", recv_pkt->msg_read_cnt, recv_pkt->msg);
                	fwrite((void*)recv_pkt->msg, sizeof(char), recv_pkt->msg_read_cnt, fp);    //buffer를 통해 fp(receive 파일로) fwrite
                    if(recv_pkt->file_eof == 1) break;
                }
                printf("file read 완료. \n");
                fflush(stdout);
                fclose(fp);

                break;

            case 4:   //내 파일 업로드
                break;
            
            default:
                printf("Wrong Input. (%d)", recv_pkt->option);
                break;
        }
	}
        
	close(sd);
	return 0;
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}