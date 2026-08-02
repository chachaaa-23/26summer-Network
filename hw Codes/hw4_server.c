#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#define BUF_SIZE 512
#define MAX_CLNT 100
#define MAX_DBSIZE 30 
#define COLOR_TEXT "\033[38;2;36;157;143m"
#define COLOR_RESET "\033[0m"

typedef struct {
    char r_word[100];       //연관검색어 related word
    int s_count;            //검색횟수 search count
    int w_start;            //검색어 시작 인덱스
} word;

typedef struct{
    char search_word[100];        //user 검색어
    word related_word[100];       //연관검색어 정보
    int rword_cnt;                //총 연관검색어 수 related word count
} pkt_t;

int clnt_cnt=0;     //서버에 접속한 클라이언트 소켓 관리용 변수
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutx;
char file_name[20];
word dbword[MAX_DBSIZE];
int db_tot_cnt=0;

void* handle_clnt(void* arg);
void cal_words(pkt_t* recv, pkt_t* send);
void error_handling(char* msg);
int desc_cmp(const void* a, const void* b);

int main(int argc, char* argv[]){
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    pthread_t t_id;
    char buf[BUF_SIZE];

    strcpy(file_name, argv[2]);
    if(argc != 3){
        printf("Usage : %s <port> <db_file.txt>\n", argv[0]);
        exit(1);
    }
    pthread_mutex_init(&mutx, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));
    if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    //연관검색어 데이터, 배열 저장
    FILE* fp = fopen(file_name, "rb");         // 1. txt 파일에 있는 데이터, 불러와서 배열 저장 (word dbword[])
    if(fp==NULL)
        perror("fopen failed");
    
    memset(buf, 0, sizeof(buf));

    while(fgets(buf, sizeof(buf), fp) != NULL){
        // printf(">buf: %s\n", buf);
        char* ptr = strtok(buf, ",\n");   
        strcpy(dbword[db_tot_cnt].r_word, ptr);
        printf("dbword[%d].r_word: %s\n", db_tot_cnt, dbword[db_tot_cnt].r_word);
        ptr = strtok(NULL, ",\n");    
        dbword[db_tot_cnt].s_count = atoi(ptr);
        printf("dbword[%d].s_count: %d\n", db_tot_cnt, dbword[db_tot_cnt].s_count);
        db_tot_cnt++;
    }
    printf("db read finished.\n");
    fclose(fp);

    while(1){
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);       //accept한 클라이언트 소켓,

        pthread_mutex_lock(&mutx);      //cp
        clnt_socks[clnt_cnt++] = clnt_sock;    //전역변수로 할당해 저장 (동시발생, 같은 cp)
        pthread_mutex_unlock(&mutx);

        pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);    
        pthread_detach(t_id);
        printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
    }
    close(serv_sock);
    return 0;
}

void* handle_clnt(void* arg){
    int clnt_sock = *((int*)arg);
    int str_len=0;

    //주고받을 패킷 생성
    pkt_t* send_pkt = (pkt_t*)malloc(sizeof(pkt_t));        //user 검색어와 겹치는 연관검색어 
    pkt_t* recv_pkt = (pkt_t*)malloc(sizeof(pkt_t));        //user 입력 검색어

    // while((str_len = read(clnt_sock, recv_pkt, sizeof(*recv_pkt))) != 0){      //클라이언트가 입력한 메시지 받아 (대기중)
    while(1){
    str_len = read(clnt_sock, recv_pkt, sizeof(*recv_pkt));      //클라이언트가 입력한 메시지 받아 (대기중)
    if(!str_len)    
        error_handling("Wrong read");

    printf("user keyword: %s\n", recv_pkt->search_word);

    cal_words(recv_pkt, send_pkt);
    write(clnt_sock, send_pkt, sizeof(*send_pkt)); //게산결과 보내기 write
    }
    //클라이언트에서 연결 종료 시, 반환
    pthread_mutex_lock(&mutx);      //cp
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

    free(send_pkt);
    free(recv_pkt);
    return NULL;
}

void cal_words(pkt_t* recv, pkt_t* send){     //recv에 있는 searchword와 겹치는 단어가 txt 파일에 있는지 계산, send에 정리
    send->rword_cnt=0;
    if(strlen(recv->search_word) == 0)
        return;
    strcpy(send->search_word, recv->search_word);

    // 2. db 데이터와 recv->searchword 가 겹치는 부분 있는지 확인, 
    char* tmp;
    for(int i=0; i<db_tot_cnt; i++){
        if((tmp = strstr(dbword[i].r_word, recv->search_word)) != NULL){
        // 3. 있으면 send->related_word로 저장
            int keyword_start_len = tmp-dbword[i].r_word;
            send->related_word[send->rword_cnt].w_start = keyword_start_len;     //word[i] 키워드 시작하는 index i 번호
            printf(">>send->related_word[send->rword_cnt].w_start: %d\n", send->related_word[send->rword_cnt].w_start);

            strcpy(send->related_word[send->rword_cnt].r_word, dbword[i].r_word);
            send->related_word[send->rword_cnt].s_count = dbword[i].s_count;
            printf(">>%s\n", send->related_word[send->rword_cnt].r_word);
            send->rword_cnt++;
            // printf(">tmp: %c\n", *tmp);
        }
    }
    // 4. 검색횟수로 내림차순정렬
    qsort(send->related_word, send->rword_cnt, sizeof(word), desc_cmp);
    for(int i=0; i<send->rword_cnt; i++)
        printf("~%d: %s, %d\n", i+1, send->related_word[i].r_word, send->related_word[i].s_count);
}

int desc_cmp(const void* a, const void* b){ //qsort desc sort 용도
    word* p1 = (word*)a;
    word* p2 = (word*)b;

    return (p2->s_count - p1->s_count);
}
void error_handling(char * msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}