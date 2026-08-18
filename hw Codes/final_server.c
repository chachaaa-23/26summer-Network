#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#define MAX_CLNT 256
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
int current_sec=-2;              //게임 진행 시간
int clnt_join=0;                //join 한 클라이언트 수 체크
int SIZE, i;
s_pkt* serv_pkt;               //clnt에게 보내줄 정보들
int serv_join=0;                  //server측에 보내진 정보 수 체크
int start_flag=0;               //thread,user입력 시작신호 1
int end_join=0;                 //종료신호 받기

int main(int argc, char* argv[]){
    signal(SIGPIPE, SIG_IGN);       //SIGPIPE 무시
    srand((unsigned int)time(NULL));
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    if (argc != 6) {
		printf("Usage : %s <num> <size> <board> <time> <port>\n", argv[0]);
		exit(1);
	}
    int flag = fcntl( STDIN_FILENO, F_GETFL, 0 ); 
    fcntl(STDIN_FILENO, F_SETFL, flag | O_NONBLOCK);                           //non-blocking mode
    /*게임 조건 체크*/
    for(int i=1; i<6; i++){
        if(atoi(argv[i]) == 0)
            error_handling("Input error. Arg must be nonzero\n");
    }
    if((atoi(argv[2])* atoi(argv[2])) < atoi(argv[3]))        //size < board
        error_handling("Input error. Check <size> or <board>. \n");
    if(atoi(argv[1]) % 2 == 1 )//user 수 홀수
        error_handling("Input error. <num> must be even\n");
    if(atoi(argv[3]) % 2 == 1 )//board 수 홀수
        error_handling("Input error. <board> must be even\n");

    /*1. 서버 bind*/
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
    //판, 방석위치와 뒤집힘 여부 (짝수면 red, 홀수면 blue)
    for( i=0; i<pillow_num; i++){        
        int tmp_x = rand()%SIZE;
        int tmp_y = rand()%SIZE;
        if(matrix[tmp_x][tmp_y].pillow == 0){
            if(i%2 == 0)
                matrix[tmp_x][tmp_y].pillow = 2;    //x+y좌표, 짝수면 red(2) 홀수면 blue(1) (결과계산& client측 print시)
            else 
                matrix[tmp_x][tmp_y].pillow = 1;   
            printf("matrix[%d][%d].pillow: %d\n", tmp_x, tmp_y, matrix[tmp_x][tmp_y].pillow);
        } else i--;
    }   
    for( i=0; i<player_num; i++){        
        int tmp_x = rand()%SIZE;
        int tmp_y = rand()%SIZE;
        if(matrix[tmp_x][tmp_y].player == 0){
            matrix[tmp_x][tmp_y].player = i+1;
            printf(">matrix[%d][%d].player: %d\n", tmp_x, tmp_y, matrix[tmp_x][tmp_y].player);
        }
        else i--;
    }   

    /*4. client 입력받고 실행용 스레드 생성*/
    serv_pkt = (s_pkt*)malloc(sizeof(s_pkt)* player_num);       //유저별로 들어온 변화값 저장
    pthread_t t_id[player_num];             //짝수면 red, 홀수면 blue
    
    for( i=0; i<player_num; i++){   //n명 들어올때까지 대기
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);   

        pthread_mutex_lock(&mutx);                   
        clnt_socks[clnt_cnt++] = clnt_sock;          
        pthread_mutex_unlock(&mutx);

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
            c_pkt c;
            c.x = -200;
            pthread_mutex_lock(&mutx);                   
            for(int i=0; i<clnt_cnt; i++)
                write(clnt_socks[i], &c, sizeof(c_pkt));
            // printf("게임 시작신호 전송\n");

            sleep(1);
            start_flag = 1;
            pthread_mutex_unlock(&mutx);                   

            while(1){
                /*모든 클라이언트에게 종료 신호 받을 시*/
                if(current_sec == tot_time){

                    //모든 클라이언트가 끝나면,
                    if(end_join == clnt_cnt){
                    /*종료*/
                    printf("Game Finished. (current sec: %d)\n", current_sec);
                    break;
                    }
                } 
                else {
                    sleep(1);
                    pthread_mutex_lock(&mutx);                   
                    current_sec++;
                    pthread_mutex_unlock(&mutx);                   

                    printf("Current Time: %-3d(sec)\n", current_sec);
                    printf("\x1b[%dA\r", 1);
                    fflush(stdout);   
                }
            }
            break;
        }
    }
    printf("Game Over ^__^ bb\n");

    free(start_packet);
    free(matrix);
    free(serv_pkt);
    close(serv_sock);
    close(clnt_sock);           
    return 0;
}

void* handle_clnt(void* arg){
    thread_arg tharg = *((thread_arg*)arg);
    int clnt_sock = tharg.clnt_sock;
    int client_id = tharg.clnt_id;
    int prev_x, prev_y;     //직전입력 좌표
    c_pkt c;
    int finish_flag=0;
    
    c_pkt* clnt_pkt = (c_pkt*)malloc(sizeof(c_pkt));
    game_state* init = (game_state*)malloc(sizeof(game_state));
    printf(">>my_id: %d thread created, matrix 정보 전송 시작.\n", client_id);
    
    /*1) matrix 초기정보 전송*/
    for(int j=0; j<SIZE; j++){      //전체 matrix 데이터 보내기
        for(int k=0; k<SIZE; k++){
            init->player = matrix[j][k].player;
            init->pillow = matrix[j][k].pillow;
            write(clnt_sock, init, sizeof(*init));
            if(matrix[j][k].player == client_id){       //init x, y 좌표 저장
                prev_x = j;
                prev_y = k;
            }
            // printf(">matrix[%d][%d].player: %d, matrix[%d][%d].pillow: %d\n", j, k, matrix[j][k].player, j, k, matrix[j][k].pillow);
        }
    }

    /*2) 초기정보 전송완료, clnt_join++, timer 시작 */
    pthread_mutex_lock(&mutx);                  
    printf("matrix 전송 완료\n\n");
    clnt_join++;
    pthread_mutex_unlock(&mutx);                

    /*3) 각 클라이언트에게 특정시간마다 메시지 받기 -- t초 이내 값만 받기*/
    int read_cnt;
    int error_flag=0;
    while(1){
        if(start_flag == 1){
            if(read_cnt = read(clnt_sock, clnt_pkt, sizeof(*clnt_pkt)) < 0){             //유저 입력, %% ms 내에 입력된 마지막값
                if(read_cnt < 0){
                    printf("read_cnt: %d\n", read_cnt);
                    if(errno == ECONNRESET){
                        printf("ECONNRESET occured. \n");
                        error_flag=1;
                        break;
                    }
                }
            }

            //4) 바뀐 matrix 업데이트 
            pthread_mutex_lock(&mutx);                  
            if(clnt_pkt->x == -1 && matrix[prev_x][prev_y].pillow != 0){                      //flip 시 (해당 위치에 방석 존재시)
                matrix[prev_x][prev_y].pillow++;        //기존위치에서 flip
            }else if(clnt_pkt->x >= 0){
                matrix[prev_x][prev_y].player = 0;      //이전 위치에서
                matrix[clnt_pkt->x][clnt_pkt->y].player = client_id;    //새로운 위치로 이동
            }else{
                printf("x wrong input. %d\n", clnt_pkt->x);
            }
            pthread_mutex_unlock(&mutx);                       
            
            /*바뀐 나의 유저 matrix, 서버패킷에 업데이트 후,*/
            pthread_mutex_lock(&mutx);                  
            serv_pkt[client_id-1].prev_x = prev_x;      //현 결과 덮어씌우기 전, 원래위치
            serv_pkt[client_id-1].prev_y = prev_y;  
            if(clnt_pkt->x == -1)                       //flip 신호인 경우  
                serv_pkt[client_id-1].x = prev_x;
            else
                serv_pkt[client_id-1].x = clnt_pkt->x;
            serv_pkt[client_id-1].y = clnt_pkt->y;
            serv_pkt[client_id-1].pillow = matrix[serv_pkt[client_id-1].x][serv_pkt[client_id-1].y].pillow;
            serv_pkt[client_id-1].clock = current_sec;

            if(current_sec == tot_time) {
                finish_flag=2;
                serv_pkt[client_id-1].datatype = 2;     //final
            } else serv_pkt[client_id-1].datatype = 1;
            // printf("time: %d (tot: %d) finish_flag: %d \n", current_sec, tot_time, finish_flag);
            serv_join++;
            pthread_mutex_unlock(&mutx);

            /*전부 업뎃 시, 전송*/
            while(1){ 
                // printf("join한 user %d, 총 user %d \n", serv_join, clnt_cnt);               
                if(serv_join % clnt_cnt == 0){      //모든 클라이언트,신호 업데이트 완료시
                    for(int i=0; i< clnt_cnt; i++){
                        write(clnt_socks[client_id-1], &serv_pkt[i], sizeof(s_pkt));  //나의 user에게 바뀐정보들 전송. 
                        // printf(">>data sent (%d -> %d). dtype: %d\n", client_id, i, serv_pkt[i].datatype);
                    }                
                    break;
                }           
            }
            // printf("updated (x: %d, y: %d, pillow state: %d, clock: %d, dtype: %d)\n\n", serv_pkt[client_id-1].x, serv_pkt[client_id-1].y, serv_pkt[client_id-1].pillow, serv_pkt[client_id-1].clock, serv_pkt[client_id-1].datatype);
            
            if(clnt_pkt->x != -1)
                prev_x = clnt_pkt->x;       //현 위치 업데이트
            prev_y = clnt_pkt->y;

            if(finish_flag == 2) break;
        }
    }

    if(error_flag){         //에러로 인한 종료시
        pthread_mutex_lock(&mutx);      
        for(int i=0; i<clnt_cnt; i++){
            if(clnt_sock == clnt_socks[i]){         //방금 종료한 클라이언트, 중간에 들어온 경우
                while(i++ < clnt_cnt-1)
                    clnt_socks[i] = clnt_socks[i+1];
                break;
            }
        }
        clnt_cnt--;
        pthread_mutex_unlock(&mutx);
        close(clnt_sock);
    }

    while(1){       //클라이언트가 종료될때까지 기다린 뒤,
        read(clnt_sock, &c, sizeof(c_pkt));
        // printf(">>fin c: %d (i: %d)\n", c.x, i);
        if(c.x == -202){         //종료신호 read시
            pthread_mutex_lock(&mutx);
            end_join++;
            pthread_mutex_unlock(&mutx);
            break;
        }
    }


    free(init);
    free(clnt_pkt);
    printf("thread %d done\n", client_id);
    return NULL;
}

void error_handling(char * msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}