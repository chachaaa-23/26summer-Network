#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <termios.h>
#include <time.h>
#include <fcntl.h>
void error_handling(char* msg);
void printout();
int getch();

#define BUF_SIZE 128
#define COLOR_TEXT_B "\033[38;2;227;242;253m"
#define COLOR_BG_B "\033[48;2;33;150;243m"
#define COLOR_TEXT_R "\033[38;2;220;20;60m"
#define COLOR_BG_R "\033[48;2;247;202;201m"
#define COLOR_RESET "\033[0m"

typedef struct {
    int player_num;     //총 n명
    int player_id;      //플레이어 id
    int size;           //s*s 판
    int clock;          //게임 진행시간
}start_pkt; 
typedef struct{
    int player;         //플레이어 존재 시 id, 아무도 없으면 0     
    int pillow;
} game_state;
typedef struct {    
    int prev_x, prev_y;     //업데이트 전, 기존 위치
    int x, y;               //업데이트 할 user location
    int pillow;
    int clock;              //남은 게임시간
    int datatype;           // -1 init, 0 start, 1 playing, 2 end
} s_pkt;
typedef struct{
    int x, y;           //x가 -1 == Enter 입력
} c_pkt;    

game_state** matrix;        //게임 정보담는 matrix
int tot_time;                   //게임 제한 시간
int current_sec=0;          //게임 진행 시간
int SIZE;                   //게임 s*s

int main(int argc, char* argv[]){
    int sock;
    struct sockaddr_in serv_addr;
    int myid, i;       //s*s 판 크기, id
    char tmpword;
    int prev_x, prev_y;

    start_pkt* start_packet = (start_pkt*)malloc(sizeof(start_pkt));    //초기 게임 세팅 시 받아옴
    game_state* init = (game_state*)malloc(sizeof(game_state));         //초기 matrix 세팅 시 받아옴
    s_pkt* serv_pkt = (s_pkt*)malloc(sizeof(s_pkt));                    //서버, 업데이트 내용 받아옴
    c_pkt* clnt_pkt = (c_pkt*)malloc(sizeof(c_pkt));                    //클라이언트, 유저 입력한 내용 전달

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);                           //non-blocking mode

    /*1. 서버와 소켓연결*/
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

    /*2. matrix 초기정보 받기*/
    read(sock, start_packet, sizeof(*start_packet));       //matrix 초기정보 받기

    myid = start_packet->player_id;
    SIZE = start_packet->size;
    tot_time = start_packet->clock;
    printf("game init info received. (player num: %d, playser id: %d, 판 size: %d, 게임 tot_time: %d)\n", start_packet->player_num, start_packet->player_id, start_packet->size, start_packet->clock);

    matrix = (game_state**)malloc(sizeof(game_state*)* SIZE);   //matrix malloc
    for( i=0; i<SIZE; i++)
        matrix[i] = (game_state*)malloc(sizeof(game_state)* SIZE);

    /*3. matrix 내용물 받기*/
    for( i=0; i<SIZE; i++){
        for(int j=0; j<SIZE; j++){
            read(sock, init, sizeof(*init));
            // printf("init->pillow: %d\n",init->pillow);
            // printf(">init->player: %d\n",init->player);
            matrix[i][j].player = init->player;
            matrix[i][j].pillow = init->pillow;
            if(matrix[i][j].player == myid){
                prev_x = i;
                prev_y = j;
            }
            printf(">matrix[%d][%d].player: %d, matrix[%d][%d].pillow: %d\n", i,j, matrix[i][j].player, i,j, matrix[i][j].pillow);
        }
    }
    printf(">>myid: %d, 초기정보 받기 완료\n", myid);

    /*4. 게임 진행*/
    while(1){
        if(tot_time == 0) break;        //1) timeout

        // 2) user에게 방향키 non-blocking 방식 입력받아  (wasd, enter)
        // clock_t start = clock();
        tmpword = getch();              //현 상태 입력받고,  
        printf("Entered: %c", tmpword);    
        fflush(stdout);
        // while(1){
        //     tmpword = getch();              //현 상태 입력받고,  
        //     clock_t end = clock();
        //     double gap = end-start;
        //     if(gap > 300) break;
        // }
        printf("\nprev 좌표(%d, %d)\n", prev_x, prev_y);

        // 3) 패킷에 넣고
        if(tmpword == 'w' || tmpword == 119){                     //w key (up, x--)
            if(serv_pkt->x != 0) {
                clnt_pkt->x = prev_x -1;        //새로 바뀐 경로 자체값
                clnt_pkt->y = prev_y;
            }
        }
        else if(tmpword == 'a' || tmpword == 97){        //a key (left, y--)
            if(serv_pkt->y != 0) {
                clnt_pkt->x = prev_x;
                clnt_pkt->y = prev_y -1;
            }
        }
        else if(tmpword == 's' || tmpword == 115){        //s key (down, x++)
            if(serv_pkt->x != SIZE-1) {
                clnt_pkt->x = prev_x +1;
                clnt_pkt->y = prev_y ;
            }
        }
        else if(tmpword == 'd' || tmpword == 100){        //d key (right, y++)
            if(serv_pkt->y != SIZE-1) {
                clnt_pkt->x = prev_x;
                clnt_pkt->y = prev_y +1;
            }
        }
        else if(tmpword == '\n' || tmpword == 10){       //enter (filp, x == -1)            
            clnt_pkt->x = -1;
        }else{                          //미등록 키
            printf("미등록 키\n");
            clnt_pkt->x = prev_x;
            clnt_pkt->y = prev_y;
        }
        
        // 4) 서버전송 (non-blocking)
        write(sock, clnt_pkt, sizeof(*clnt_pkt));
        printf("바뀐 좌표(%d, %d)\n", clnt_pkt->x, clnt_pkt->y);
        if(clnt_pkt->x != 1){       //flip 말고 단순 움직임일 시
            prev_x = clnt_pkt->x;   //나의 player 위치 업데이트
            prev_y = clnt_pkt->y;
        }

        // 5) 서버로부터 최신 정보 받아오기 (non-blocking)
        for(int i=0; i<start_packet->player_num; i++){      //모든 플레이어에게 결과 업데이트 받기
            read(sock, serv_pkt, sizeof(*serv_pkt));
            printf("서버로부터 %d's matrix 정보, 수신완. \n(x: %d, y: %d, pillow state: %d, clock: %d)\n", i, serv_pkt->x, serv_pkt->y, serv_pkt->pillow, serv_pkt->clock);
            
            // 6) matrix에 정보 반영
            if(serv_pkt->x == -1)                       //user, 현 위치에서 flip 시
                matrix[serv_pkt->prev_x][serv_pkt->prev_y].pillow = serv_pkt->pillow;        //방석상태만 업데이트
            else {
                matrix[serv_pkt->prev_x][serv_pkt->prev_y].player = 0;      //이전 위치 지우고
                matrix[serv_pkt->x][serv_pkt->y].player = i+1;              //새로운 위치로 이동
                tot_time = serv_pkt->clock;
            }
        }
        // 7) matrix 띄우기
        printf("\033[H\033[2J\n\n\n");
        printout();

    }
    printf("Game Finished ^__^ \n");
    //게임 결과 띄우기

    free(start_packet);
    free(matrix);
    free(serv_pkt);
    free(init);
    close(sock);
    return 0;
}

void printout(){
    printf("----------------------------------------------\n");
    for(int i=0; i<SIZE; i++){
        for(int j=0 ; j<SIZE; j++ ){
            if(matrix[i][j].pillow != 0 && matrix[i][j].pillow%2 ==0 && matrix[i][j].player != 0)            //Red팀 방석이고(2, 짝수) player 존재
                printf( "|%s%2d%s|", COLOR_BG_R, matrix[i][j].player,COLOR_RESET); 

            else if(matrix[i][j].pillow != 0 && matrix[i][j].pillow%2 == 0 && matrix[i][j].player == 0)        //Red팀 방석이고(2) player 미존재
                printf("|%s  %s|", COLOR_BG_R, COLOR_RESET); 

            else if(matrix[i][j].pillow != 0 && matrix[i][j].pillow%2 == 1 && matrix[i][j].player != 0)        //blue팀 방석이고(1, 홀수) player 존재
                printf( "|%s%2d%s|", COLOR_BG_B, matrix[i][j].player,COLOR_RESET); 

            else if(matrix[i][j].pillow != 0 && matrix[i][j].pillow%2 == 1 && matrix[i][j].player == 0)        //blue팀 방석이고(1) player 미존재
                printf("|%s  %s|", COLOR_BG_B, COLOR_RESET); 

            else if(matrix[i][j].pillow == 0 && matrix[i][j].player != 0)        //방석 없고 player 존재
                printf("|%2d|", matrix[i][j].player); 
            else printf("|  |");                                                 //방석 없고 player 미존재
            fflush(stdout);
        }
        printf("\n");
    }
    printf("Current sec: %d\n", tot_time);
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
