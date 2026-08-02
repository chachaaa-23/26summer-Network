#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#define MAX_CLNT 256
#define BUF_SIZE 128

void *handle_clnt(void * arg);
// void send_msg(char * msg, int len);
void error_handling(char * msg);

typedef struct {
    int player_num;     //총 n명
    int player_id;      //플레이어 id
    int size;           //s*s 판
    int clock;          //게임 진행시간
}start_pkt; 
typedef struct{
    int player;       
    int pillow;
} game_state;

typedef struct {    
    int prev_x, prev_y;     //업데이트 전, 기존 위치
    int x, y;           //user location
    int pillow;
    int clock;          //남은 게임시간
    int datatype;       //0 start, 1 playing, 2 end
} s_pkt;                //server packet
typedef struct{
    int x, y;           //x 음수 == Enter 입력
} c_pkt;                //client packet
typedef struct{
    int clnt_sock;
    int clnt_id;
}thread_arg;

int clnt_cnt=0;         //서버에 접속한 클라이언트 소켓 관리용 변수
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutx;
game_state** matrix;
int tot_time;                   //게임 제한 시간
int current_sec=1;              //게임 진행 시간
int clnt_join=0;                //join 한 클라이언트 수 체크
int SIZE, i;

int main(int argc, char* argv[]){
    srand((unsigned int)time(NULL));
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    if (argc != 6) {
		printf("Usage : %s <num> <size> <board> <time> <port>\n", argv[0]);
		exit(1);
	}
    /*1. 서버 bind*/
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);                           //non-blocking mode
    pthread_mutex_init(&mutx, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[5]));
    if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    /*2. 게임 정보 설정 (n명, s*s 칸, b개 방석, t초, 포트p)*/
    int player_num = atoi(argv[1]);
    SIZE = atoi(argv[2]);
    int pillow_num = atoi(argv[3]);
    tot_time = atoi(argv[4]);

    start_pkt* start_packet = (start_pkt*)malloc(sizeof(start_pkt));
    start_packet->player_num = player_num;
    //player_id, client 입력받는 순서대로 저장
    start_packet->size = SIZE;
    start_packet->clock = -1;       //시작 x
    matrix = (game_state**)malloc(sizeof(game_state*)* SIZE);
    for( i=0; i<SIZE; i++)
        matrix[i] = (game_state*)malloc(sizeof(game_state)* SIZE);


    /*3. 초기 방석& 유저위치 random 설정*/
    //판, 방석위치와 뒤집힘 여부 표시 (짝수면 red, 홀수면 blue)
    for( i=0; i<pillow_num; i++){        //랜덤한 matrix에 방석& 유저위치 할당
        int tmp_x = rand()%SIZE;
        int tmp_y = rand()%SIZE;
        if(matrix[tmp_x][tmp_y].pillow == 0){
            if(i%2 == 0)
                matrix[tmp_x][tmp_y].pillow = 2;    //x+y좌표, 짝수면 red(2) 홀수면 blue(1) (결과계산& client측 print시)
            else 
                matrix[tmp_x][tmp_y].pillow = 1;    //x+y좌표, 짝수면 red(2) 홀수면 blue(1) (결과계산& client측 print시)
            printf("matrix[%d][%d].pillow: %d\n", tmp_x, tmp_y, matrix[tmp_x][tmp_y].pillow);
        } else i--;
    }   
    for( i=0; i<player_num; i++){        //랜덤한 matrix에 유저위치 할당
        int tmp_x = rand()%SIZE;
        int tmp_y = rand()%SIZE;
        if(matrix[tmp_x][tmp_y].player == 0){
            matrix[tmp_x][tmp_y].player = i+1;
            printf(">matrix[%d][%d].player: %d\n", tmp_x, tmp_y, matrix[tmp_x][tmp_y].player);
        }
        else i--;
    }   

    /*4. client 입력받고 실행용 스레드 생성*/
    pthread_t t_id[player_num];             //짝수면 red, 홀수면 blue팀
    //n명 들어올때까지 대기
    for( i=0; i<player_num; i++){
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);   //accept 한 클라이언트 소켓,

        pthread_mutex_lock(&mutx);                   
        clnt_socks[clnt_cnt++] = clnt_sock;          //전역변수 할당
        pthread_mutex_unlock(&mutx);

        //각 client 스레드 생성
        start_packet->player_id = i+1;  //i번째 client에게 전역변수 패킷 보내기
        thread_arg tharg;
        tharg.clnt_sock = clnt_sock;
        tharg.clnt_id = start_packet->player_id;

        pthread_create(&t_id[i], NULL, handle_clnt, (void*)&tharg);   
        pthread_detach(t_id[i]);
        printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));

        write(clnt_socks[i], start_packet, sizeof(*start_packet));
        printf("start_packet(id: %d) 초기데이터 전송 완료.\n", start_packet->player_id);   
    }

    /*6. 시간 측정*/
    while(1){
        if(clnt_join == clnt_cnt){
            if(current_sec == tot_time){
                printf("Game Finished. (current sec: %d)\n", current_sec);
                break;
            } else {
                sleep(1);
                pthread_mutex_lock(&mutx);                   
                current_sec++;
                pthread_mutex_unlock(&mutx);                   

                printf("Current Time: %-3d(sec)\n", current_sec);
                printf("\x1b[%dA\r", 1);
                fflush(stdout);   
            }
        }
    }

    printf("Game Over ^__^ bb\n");
    /*7. matrix 계산, 승패여부 표시*/

    free(start_packet);
    free(matrix);
    close(serv_sock);
    return 0;
}

void* handle_clnt(void* arg){
    thread_arg tharg = *((thread_arg*)arg);
    int clnt_sock = tharg.clnt_sock;
    int str_len=0;
    int client_id = tharg.clnt_id;
    int prev_x, prev_y;     //직전입력 좌표
    s_pkt* serv_pkt = (s_pkt*)malloc(sizeof(s_pkt));
    c_pkt* clnt_pkt = (c_pkt*)malloc(sizeof(c_pkt));
    game_state* init = (game_state*)malloc(sizeof(game_state));
    printf(">>my_id: %d thread created, matrix 정보 전송 시작.\n", client_id);
    
    /*1) matrix 초기정보 전송*/

    for(int j=0; j<SIZE; j++){      //전체 matrix 데이터 보내기
        for(int k=0; k<SIZE; k++){
            init->player = matrix[j][k].player;
            init->pillow = matrix[j][k].pillow;
            write(clnt_sock, init, sizeof(*init));
            // printf("init->pillow: %d\n",init->pillow);
            // printf(">init->player: %d\n",init->player);
            if(matrix[j][k].player == client_id){       //init x, y 좌표 저장
                prev_x = j;
                prev_y = k;
            }
            printf(">matrix[%d][%d].player: %d, matrix[%d][%d].pillow: %d\n", j, k, matrix[j][k].player, j, k, matrix[j][k].pillow);
        }
    }
    /*2) 초기 판 정보 전송완료, clnt_join++, timer 시작 */
    pthread_mutex_lock(&mutx);                  //cp
    printf("matrix 전송 완료\n\n");
    clnt_join++;
    pthread_mutex_unlock(&mutx);                //cp

    /*3) 각 클라이언트에게 특정시간마다 메시지 받기 -- t초 이내 값만 받기*/
    while(1){
        read(clnt_sock, clnt_pkt, sizeof(*clnt_pkt));             //유저 입력, %% ms 내에 입력된 마지막값
        
        //4) 바뀐 matrix 업데이트 
        pthread_mutex_lock(&mutx);                  //cp
        if(clnt_pkt->x == -1)                       //user, 현 위치에서 flip 시
            matrix[prev_x][prev_y].pillow++;        //방석상태만 업데이트
        else {
            matrix[prev_x][prev_y].player = 0;      //이전 위치에서
            matrix[clnt_pkt->x][clnt_pkt->y].player = client_id;    //새로운 위치로 이동
        }
        printf(">>prev (%d, %d)\n", prev_x, prev_y);
        printf(">>now (%d, %d)\n", clnt_pkt->x, clnt_pkt->y);
        pthread_mutex_unlock(&mutx);                  //cp        
        
        /*바뀐 matrix, 모든 클라이언트에게 결과 전송*/
        serv_pkt->prev_x = prev_x;
        serv_pkt->prev_y = prev_y;
        serv_pkt->x = clnt_pkt->x;
        serv_pkt->y = clnt_pkt->y;
        serv_pkt->pillow = matrix[clnt_pkt->x][clnt_pkt->y].pillow;
        serv_pkt->clock = current_sec;
        serv_pkt->datatype = 1;
        for(int i=0; i< clnt_cnt; i++){
            write(clnt_socks[i], serv_pkt, sizeof(*serv_pkt));  //모든 클라이언트에게 바뀐 정보 전송. 
            printf(">>data sent (%d -> %d)\n", client_id, i);
        }   

        printf("updated. (x: %d, y: %d, pillow state: %d, clock: %d)\n\n", serv_pkt->x, serv_pkt->y, serv_pkt->pillow, serv_pkt->clock);
        prev_x = clnt_pkt->x;       //현 위치 업데이트
        prev_y = clnt_pkt->y;
    }

    // 게임 시간 초과 시, blocking 탈출-> read가 0 반환 == 클라이언트에서 연결 종료함
    pthread_mutex_lock(&mutx);      //cp
    for(int i=0; i<clnt_cnt; i++){                                      //remove disconnected client
        if(clnt_sock == clnt_socks[i]){                             //방금 종료한 클라이언트가 중간에 들어온 클라이언트라면
            while(i++ < clnt_cnt-1)
                clnt_socks[i] = clnt_socks[i+1];                    //한칸씩 배열 당기기
            break;
        }
    }
    clnt_cnt--;
    pthread_mutex_unlock(&mutx);

    free(serv_pkt);
    free(init);
    close(clnt_sock);           
    return NULL;
}

void error_handling(char * msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}