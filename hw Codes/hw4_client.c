#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <termios.h>
#define BUF_SIZE 512
#define COLOR_TEXT "\033[38;2;36;157;143m"
#define COLOR_BG "\033[48;2;36;157;143m"
#define COLOR_RESET "\033[0m"

void error_handling(char* msg);
int getch();

typedef struct {
    char r_word[100];
    int s_count;
    int w_start;            //검색어 시작 인덱스
} word;

typedef struct{
    char search_word[100];
    word related_word[100];
    int rword_cnt;              //총 연관검색어 수 related word count
} pkt_t;

int main(int argc, char* argv[]){
    int sock;
    struct sockaddr_in serv_addr;

    if(argc != 3){
		printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }
    sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    pkt_t* send_pkt = (pkt_t*)malloc(sizeof(pkt_t));        //user 검색어와 겹치는 연관검색어 
    pkt_t* recv_pkt = (pkt_t*)malloc(sizeof(pkt_t));        //user 입력 검색어

    char myword[BUF_SIZE]="";
    char tmpword;
    int i=0;            //문자열 길이.
    int flag=0;         //finish == 1

    printf("%sSearch Word:%s ", COLOR_BG, COLOR_RESET);  
    while(1){
        tmpword = getch();        //현 상태 입력받고, 
        printf("\033[H\033[2J");

        if(tmpword == 127){      //1. del key 입력& 이전 입력 존재시
            if(i > 0){
                i--;
                myword[i] = '\0';   //기존 글자 지우고,
                printf("\b \b");
                fflush(stdout);  
            }
        }
        else if(tmpword == '\n'){   //2. enter-> 이번명령 이후 exit            
            myword[i] = tmpword;
            i++;
            myword[i] = '\0';
            flag = 1;
        }else{      //3. normal input
            myword[i] = tmpword;
            fflush(stdout);  
            i++;
            myword[i] = '\0';
        }
        printf("\n\n%sSearch Word:%s ", COLOR_BG, COLOR_RESET);  
        printf("%s", myword);  
        fflush(stdout);  

        //서버에게 현 검색어 보내(write), 연관검색어 처리
        strcpy(send_pkt->search_word, myword);
        write(sock, send_pkt, sizeof(*send_pkt));

        //일치여부 업데이트 받아(read),
        read(sock, recv_pkt, sizeof(*recv_pkt));
        //연관검색어 출력
        printf("\n------------------------------------------------------------\n");    

        int willprint_cnt = recv_pkt->rword_cnt;
        if(willprint_cnt > 10)
            willprint_cnt=10;
        for(int j=0; j<willprint_cnt; j++){
            //키워드 출력
            int kword_start_idx = recv_pkt->related_word[j].w_start;    //키워드랑 겹치는 연관검색어 시작인덱스
            int kword_len = strlen(myword);                             //키워드 길이
            int rword_len = strlen(recv_pkt->related_word[j].r_word);   //연관검색어 길이
            int plain_color_idx = kword_start_idx + kword_len;          //색칠 후 일반글씨색 시작인덱스

            if(kword_start_idx != 0){    //맨처음에 키워드가 아닌경우
                for(int k=0; k<kword_start_idx; k++){   //일반 색처리
                    printf("%c", recv_pkt->related_word[j].r_word[k]);
                }
            }
                for(int k=0; k<kword_len; k++){                   //키워드 부분 색처리
                    printf("%s%c%s", COLOR_TEXT, recv_pkt->related_word[j].r_word[k+ kword_start_idx], COLOR_RESET);
                }
            for(int k=plain_color_idx; k<rword_len; k++){      //일반 색처리
                printf("%c", recv_pkt->related_word[j].r_word[k]);
            }
            printf(" (%d)\n", recv_pkt->related_word[j].s_count);
        }    

        if(flag) break;
        fflush(stdout);
    }
    
    free(send_pkt);
    free(recv_pkt);
    close(sock);
    return 0;
}

int getch(){  
  char ch;  
  struct termios buf;  
  struct termios save;  

   tcgetattr(STDIN_FILENO, &save);  
   buf = save;  
   buf.c_lflag &= ~(ICANON|ECHO);    

   buf.c_cc[VMIN] = 1;  
   buf.c_cc[VTIME] = 0;  
   tcsetattr(STDIN_FILENO, TCSANOW, &buf);  
    ch = getchar();
   tcsetattr(STDIN_FILENO, TCSANOW, &save);  
   return ch;  
}  

void error_handling(char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
