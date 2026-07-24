#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>


#define BUF_SIZE 1024
#define OPSZ 4
void error_handling(char* message);

int main(int argc, char* argv[]){
    int serv_sock, clnt_sock;
    char msg[BUF_SIZE];
    int opnd_cnt, i;
    int recv_cnt, recv_len;
    struct sockaddr_in serv_adr, clnt_adr;
    char filename[100];

    socklen_t clnt_adr_sz;
        if(argc!=2){
		printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock=socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock==-1)
        error_handling("socket() error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family=AF_INET;
    serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);
    serv_adr.sin_port=htons(atoi(argv[1]));

    if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) ==-1)
        error_handling("bind() error");
    if(listen(serv_sock, 5)==-1)
        error_handling("listen() error");
    clnt_adr_sz=sizeof(clnt_adr);

    for(i=0; i<5; i++){
        memset(msg, 0, sizeof(msg));
        opnd_cnt=0;
        clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);

        /*현재 디렉토리의 모든 파일 목록, 알아내서, 문자열에 저장*/
        DIR* dir = opendir("./");
        if(dir==NULL){
            perror("failed to open dir\n");
            return 0;
        }

        struct dirent* entry;
        int msglen=0;
        while((entry=readdir(dir)) != NULL){
            if(entry->d_type == DT_REG){
                strcat(msg, entry->d_name);
                strcat(msg, "\n");              // 파일 구분
            }
        }
        msglen = strlen(msg);

        write(clnt_sock, &msglen, sizeof(msglen));       //msg 길이 
        write(clnt_sock, msg, msglen);
        //printf(">>filelist: %s", msg);

        //2. read filename to get
        memset(filename, 0, sizeof(filename));
        read(clnt_sock, filename, 100);
        printf(">>entered filename ::%s::\n", filename);

        //3. find if file exists
        int read_cnt;
        
        if(strstr(msg, filename) != NULL){      //msg 버퍼에 있는 문자열과 filename 중 일치 o,
            // 해당 파일 내용, 전송. 
            memset(msg, 0, sizeof(msg));
            FILE* fp = fopen(filename, "rb");
            while(1){
                read_cnt = fread((void*)msg, 1, BUF_SIZE, fp);   //현재 파일 포인터부터 500씩 읽어서 msg에 저장해라. 몇개의 항목을 읽었는지 read_cnt에 저장해라.
                if(read_cnt < BUF_SIZE){
                    write(clnt_sock, msg, read_cnt);
                    //printf(">>msg: %.*s\n", read_cnt, msg);
                    break;
                }
                write(clnt_sock, msg, BUF_SIZE);
                //printf(">>msg: %.*s\n", read_cnt, msg);

            }
            printf("file 전송 완료. fclose.\n\n");
	        fclose(fp);
        } else{
            perror("File name incorrect. Enter again. \n\n");
        }
        close(clnt_sock);
    }
    close(serv_sock);
    return 0;
}  



void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}