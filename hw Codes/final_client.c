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
void result_print();
void sig_handler(int signo);

#define COLOR_BG_B "\033[48;2;33;150;243m"
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
    int x, y;           //x가 -1 == Enter 입력. x=-200일시 start.
} c_pkt;    

game_state** matrix;        //게임 정보담는 matrix
int tot_time;                   //게임 제한 시간
int current_sec=0;          //게임 진행 시간
int SIZE;                   //게임 s*s
s_pkt* serv_pkt;                   //서버, 업데이트 내용 받아옴

int main(int argc, char* argv[]){
    int sock;
    struct sockaddr_in serv_addr;
    int myid, i, read_cnt;       //s*s 판 크기, id
    char tmpword;
    int prev_x, prev_y;

    start_pkt* start_packet = (start_pkt*)malloc(sizeof(start_pkt));    //초기 게임 세팅
    game_state* init = (game_state*)malloc(sizeof(game_state));         //초기 matrix 세팅
    c_pkt* clnt_pkt = (c_pkt*)malloc(sizeof(c_pkt));                    //유저들 입력한 내용
    struct termios buf;  
    struct termios save;  
    tcgetattr(STDIN_FILENO, &save);  
    buf = save;
    buf.c_lflag &= ~(ICANON|ECHO);    
    buf.c_cc[VMIN] = 1;  
    buf.c_cc[VTIME] = 0;  
    tcsetattr(STDIN_FILENO, TCSANOW, &buf); 

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
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);                 //non-blocking mode

    /*2. matrix 초기정보 받기*/
    read(sock, start_packet, sizeof(*start_packet));       

    myid = start_packet->player_id;
    SIZE = start_packet->size;
    tot_time = start_packet->clock;
    printf("game init info received. (player num: %d, playser id: %d, 판 size: %d, 게임 tot_time: %d)\n", start_packet->player_num, start_packet->player_id, start_packet->size, start_packet->clock);

    c_pkt c;
    char color[5];
    if(myid%2 == 0) strcpy(color, "Red");      //짝수면 red
    else strcpy(color, "Blue");

    matrix = (game_state**)malloc(sizeof(game_state*)* SIZE);  
    for( i=0; i<SIZE; i++)
        matrix[i] = (game_state*)malloc(sizeof(game_state)* SIZE);
    serv_pkt = (s_pkt*)malloc(sizeof(s_pkt)* start_packet->player_num);                

    /*3. matrix 내용물 받기*/
    for( i=0; i<SIZE; i++){
        for(int j=0; j<SIZE; j++){
            read(sock, init, sizeof(*init));
            matrix[i][j].player = init->player;
            matrix[i][j].pillow = init->pillow;
            if(matrix[i][j].player == myid){
                prev_x = i;
                prev_y = j;
            }
        }
    }

    //게임 start counting
    while(1){
        read(sock, &c, sizeof(c_pkt));
        if(c.x == -200){         //시작신호 read시
            for(int i=3; i>0; i--){
                printf("You are <%s> Team! Game starts soon... %d\n", color, i);
                sleep(1);
                printf ("\x1b[%dA", 1);	
            }
            break;
        }
    }

    /*4. 게임 진행*/
    while(1){
        printout();

        // 2) user에게 방향키 non-blocking 방식 입력받아  (wasd, enter)
        clock_t start = clock();
        while(1){
            read_cnt = read(STDIN_FILENO, &tmpword, sizeof(tmpword));              //현 상태 입력받고,  
            clock_t end = clock();
            double gap = (double)(end-start) / CLOCKS_PER_SEC;
            if(gap > 0.1) break;            //대기시간 초과시 탈출
            fflush(stdout);
        }
            // 3) 패킷에 넣고
            if(tmpword == 'w' || tmpword == 119){                     //w key (up, x--)
                if(prev_x  > 0) {
                    clnt_pkt->x = prev_x -1;        //새로 바뀐 경로 자체값
                    clnt_pkt->y = prev_y;
                }
            }
            else if(tmpword == 'a' || tmpword == 97){        //a key (left, y--)
                if(prev_y > 0) {
                    clnt_pkt->x = prev_x;
                    clnt_pkt->y = prev_y -1;
                }
            }
            else if(tmpword == 's' || tmpword == 115){        //s key (down, x++)
                if(prev_x < SIZE-1) {
                    clnt_pkt->x = prev_x +1;
                    clnt_pkt->y = prev_y ;
                }
            }
            else if(tmpword == 'd' || tmpword == 100){        //d key (right, y++)
                if(prev_y < SIZE-1) {
                    clnt_pkt->x = prev_x;
                    clnt_pkt->y = prev_y +1;
                }
            }
            else if(tmpword == '\n' || tmpword == 10){       //enter (filp, x == -1)            
                clnt_pkt->x = -1;
            }else{                          //미등록 키 (따로 입력한 키 없을 시)
                // printf("undefined key\n");
                clnt_pkt->x = prev_x;       //기존 위치 전송. 
                clnt_pkt->y = prev_y;
            }
        
        // 4) 서버전송 (non-blocking)
        write(sock, clnt_pkt, sizeof(*clnt_pkt));

        // 5) 서버로부터 최신 정보 받아오기 (non-blocking)
        for(int i=0; i<start_packet->player_num; i++){      //모든 플레이어에게 결과 업데이트 받기
            int recv_len=0;
            read(sock, &serv_pkt[i], sizeof(s_pkt));
            
            // 6) matrix에 정보 반영
            matrix[serv_pkt[i].prev_x][serv_pkt[i].prev_y].player = 0;      //이전 위치 지우고
            matrix[serv_pkt[i].x][serv_pkt[i].y].player = i+1;              //새로운 위치로 이동
            matrix[serv_pkt[i].x][serv_pkt[i].y].pillow = serv_pkt[i].pillow;
            current_sec = serv_pkt[i].clock;
        }
        //현위치 업데이트
        if(clnt_pkt->x != -1){              //flip 말고 단순 움직임일 시
            prev_x = serv_pkt[myid-1].x;   //움직임 전 위치 업데이트
            prev_y = serv_pkt[myid-1].y;
        }

        //game over시
        if(serv_pkt[myid-1].datatype == 2) {
            break;
        }

        tmpword=0;
        printf("\x1b[%dA\x1b[0J", SIZE + 3);
        fflush(stdout);
    }
    printf("Game Finished ^__^ \n");
    result_print();                 //게임 결과 띄우기

    c.x = -202;     //end signal
    write(sock, &c, sizeof(c_pkt));

    tcsetattr(STDIN_FILENO, TCSANOW, &buf);  
    free(start_packet);
    free(matrix);
    free(serv_pkt);
    free(init);
    close(sock);
    return 0;
}

void printout(){
    for(int i=0; i<SIZE; i++)
        printf("----");
    printf("\n");
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
        }
        printf("\n");
    }
    for(int i=0; i<SIZE; i++)
        printf("----");
    printf("\nCurrent sec: %d\n", current_sec);
    fflush(stdout);
}

void result_print(){
    int red=0;
    int blue=0;
    printf("----------------------------------------------\n");
    for(int i=0; i<SIZE; i++){
        for(int j=0 ; j<SIZE; j++ ){
            if(matrix[i][j].pillow != 0 && matrix[i][j].pillow%2 == 0)  //red 방석일시
                red++;
            if(matrix[i][j].pillow != 0 && matrix[i][j].pillow%2 == 1)  //blue 방석일시
                blue++;
        }
    }
    if(red > blue) printf("Red Team Win!\n");
    else if(red < blue) printf("Blue Team Win!\n");
    else printf("Draw...\n");
    printf("\nRed: %d, Blue: %d\n", red, blue);
}

void error_handling(char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
