#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024
#define RLT_SIZE 4
#define OPSZ 4
void error_handling(char* message);

int main(int argc, char* argv[]){
    int sock;
    char msg[BUF_SIZE];
    int opnd_cnt, i;
    
    int filelist_len;

    struct sockaddr_in serv_adr;
    if(argc!=3){
		printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    sock= socket(PF_INET, SOCK_STREAM, 0);
    if(sock==-1)
        error_handling("socket() error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family=AF_INET;
    serv_adr.sin_addr.s_addr=inet_addr(argv[1]);
    serv_adr.sin_port=htons(atoi(argv[2]));

    /* 나의 클라이언트 소켓을 서버 소켓과 연결한다 (+서버 소켓 주소길이)*/
    if(connect(sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr))==-1)
        error_handling("connect() error!");
    else
        puts("connected..........");

    memset(msg, 0, sizeof(msg));

    /*나의 입력 소켓에 받아왔던 데이터를 result 변수로 받아온다 (+데이터 byte 길이) */
    read(sock, &filelist_len, RLT_SIZE);

    /*나의 입력 소켓에 있는 filelist, msg배열로 받아옴 (+message byte 길이. 전체 배열의 길이 x) */
    read(sock, &msg, filelist_len);
    printf("\nfile list: \n%s\n", msg);

    /*받아올 파일 데이터의 이름, user에게 입력받고 server에게 전송*/
    memset(msg, 0, sizeof(msg));
    printf("enter file name to read: ");
    scanf("%s", msg);

    //printf(">>strlen(msg): %d\n", strlen(msg));
    write(sock, msg, strlen(msg));
    //printf(">>filename: %s\n", msg);

    /*파일 데이터, msg 버퍼로 받아온 뒤, .dat로 저장*/
    memset(msg, 0, sizeof(msg));
    int read_cnt;
    FILE* fp = fopen("receive_hw1.dat", "wb");

    while((read_cnt = read(sock, msg, BUF_SIZE)) != 0){
        fwrite((void*)msg, 1, read_cnt, fp);
        //printf("%.*s", read_cnt, msg);
    }

	puts("Received file data\n");
    
	fclose(fp);
    close(sock);
    return 0;
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}