#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <limits.h>
#include <dirent.h>

#define BUF_SIZE 1024
#define EPOLL_SIZE 50
#define MAX_CLNT 10
void error_handling(char *message);
int get_file_size(char* path);

typedef struct {
    int option;     //1-4
    int msg_read_cnt;
    int file_eof;   //파일의 끝 여부.  0==메시지 남음, 1==메시지 끝
    char msg[BUF_SIZE];
} pkt_t;

int main(int argc, char *argv[]){
	int serv_sd, clnt_sd;
	struct sockaddr_in serv_adr, clnt_adr;
	socklen_t adr_sz;
	char buf[BUF_SIZE];
	int str_len, i, fd_num, read_cnt;
	struct epoll_event* ep_events;
	struct epoll_event event;
	int epfd, event_cnt;
    char path[MAX_CLNT][50];                  //my path- 10 client 제한

	if (argc != 2) {
		printf("Usage: %s <port>\n", argv[0]);
		exit(1);
	}
	serv_sd = socket(PF_INET, SOCK_STREAM, 0);   
	if(serv_sd == -1)
		error_handling("socket() error");
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));
	if(bind(serv_sd, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
		error_handling("bind() error");
	if((listen(serv_sd, 5)) == -1)
		error_handling("listen() error");;
	
	//epoll 생성, 조작
	epfd=epoll_create(EPOLL_SIZE);
	ep_events=malloc(sizeof(struct epoll_event) *EPOLL_SIZE);

	event.events=EPOLLIN;			//수신할 데이터 존재 시
	event.data.fd=serv_sd;		//서버소켓, event 등록
	epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sd, &event);		//서버소켓, event 등록

    pkt_t* send_pkt = (pkt_t*)malloc(sizeof(pkt_t));
    pkt_t* recv_pkt = (pkt_t*)malloc(sizeof(pkt_t));
    for(i=0; i<MAX_CLNT; i++)
        getcwd(path[i], sizeof(path));
    printf("initial working dir: %s\n", path[0]);

	while(1){
		event_cnt=epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);		//이벤트 발생한 fd detect
		if(event_cnt==-1){
			printf("epoll_wait() error");
			break;
		}

		for(i=0; i<event_cnt; i++){
			if(ep_events[i].data.fd==serv_sd){		//서버소켓, read 상태변화-
				adr_sz=sizeof(clnt_adr);
				clnt_sd=accept(serv_sd, (struct sockaddr*)&clnt_adr, &adr_sz);		//클라이언트와 sd 연결
				event.events=EPOLLIN;				//input buf 상태변화시 추적 등록
				event.data.fd=clnt_sd;
				epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sd, &event);
				printf("\nconnected client: %d\n", clnt_sd);
			}
			else{			//클라이언트로부터 데이터 받을 시
				memset(buf, 0, sizeof(buf));
				str_len=read(ep_events[i].data.fd, recv_pkt, sizeof(*recv_pkt));

                    printf("\noption received... (op: %d)\n", recv_pkt->option);
                    // printf(">>recv_pkt->msg: %s\n", recv_pkt->msg);
                    // printf(">>recv_pkt->msg_read_cnt: %d\n", recv_pkt->msg_read_cnt);
                    // printf(">>recv_pkt->file_eof: %d\n", recv_pkt->file_eof);
                    // printf("-------------------------------------------\n");

				if(str_len==0){				//EOF
					epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);
					close(ep_events[i].data.fd);
					printf("closed client: %d \n", ep_events[i].data.fd);
				}
				else{		//실제 데이터 들어올 시 
                    int path_index = (ep_events[i].data.fd-serv_sd)/2;
                    // 들어온 데이터 옵션에 따라 명령실행 
                    printf("\n>>received msg from client\n");
                    // printf(">>i: %d, serv_sd: %d, ep_events[i].data.fd: %d, path_index: %d\n", i, serv_sd, ep_events[i].data.fd, path_index);
                    memset(send_pkt->msg, 0, sizeof(send_pkt->msg));

                    switch(recv_pkt->option){       //입력에 맞게 send_pkt 작성
                        case 1:   //서버의 현 디렉토리 정보출력 
                            send_pkt->option = 1;

                            // 1. 서버 현 디렉토리 
                            snprintf(send_pkt->msg, sizeof(send_pkt->msg), "current working directory: %s\n", path[path_index]);
                            
                            // 2. 해당 디렉토리의 파일정보
                            DIR* dir = opendir(path[path_index]);       /*현재 디렉토리의 모든 파일 목록, 알아내서, 문자열에 저장*/
                            if(dir==NULL){
                                perror("failed to open dir\n");
                                return 0;
                            }
                            struct dirent* entry;
                            while((entry=readdir(dir)) != NULL){
                                memset(buf, 0, sizeof(buf));    //디렉토리 내 파일정보, buf에 작성 후, send pkt msg에 작성
                                if(entry->d_type == DT_REG){
                                    char full_name[400] = "";
                                    snprintf(full_name, sizeof(full_name), "%s/%s", path[path_index], entry->d_name);
                                    int f_size = get_file_size(full_name);

                                    snprintf(buf, sizeof(buf), "file: %s, file size: %d\n", entry->d_name, f_size);
                                    strcat(send_pkt->msg, buf);                                
                                }
                            }
                            send_pkt->msg_read_cnt = strlen(send_pkt->msg);

                            write(ep_events[i].data.fd, send_pkt, sizeof(*send_pkt));
                            break;

                        case 2:   //서버의 현 디렉토리 이동
                            send_pkt->option = 2;

                            memset(path[path_index], 0, sizeof(path[path_index]));      //방금 클라이언트에게 받은 경로로 덮어쓰기
                            strcpy(path[path_index], recv_pkt->msg);
                            // printf("path[path_index] ::%s::\n", path[path_index]);

                            snprintf(send_pkt->msg, sizeof(send_pkt->msg), "Updated path: %s", path[path_index]);
                            
                            write(ep_events[i].data.fd, send_pkt, sizeof(*send_pkt));
                            break;

                        case 3:   //서버의 파일 다운
                            send_pkt->option = 3;

        					FILE* fp = fopen(recv_pkt->msg, "rb");
                            if(fp == NULL)
                                perror("fopen failed");

                            while(1){
                                memset(send_pkt->msg, 0, sizeof(send_pkt->msg));

                                send_pkt->msg_read_cnt = fread((void*)send_pkt->msg, 1, BUF_SIZE, fp);
                                if(send_pkt->msg_read_cnt < BUF_SIZE){
                                    send_pkt->file_eof = 1;

                                    write(ep_events[i].data.fd, send_pkt, sizeof(*send_pkt));

                                    // printf("\n>>send_pkt->msg_read_cnt: %d\n", send_pkt->msg_read_cnt);
                                    printf("\n>>send_pkt->msg: %s\n", send_pkt->msg);
                                    // printf(">>send_pkt->file_eof: %d\n", send_pkt->file_eof);
                                    break;
                                }
                                send_pkt->file_eof = 0;
                                write(ep_events[i].data.fd, send_pkt, sizeof(*send_pkt));
                                // printf("\n>>send_pkt->msg_read_cnt: %d\n", send_pkt->msg_read_cnt);
                                printf("\n>>send_pkt->msg: %s\n", send_pkt->msg);
                                // printf(">>send_pkt->file_eof: %d\n", send_pkt->file_eof);
                            }
                            printf("File Upload Completed. fclose. \n");
                            fclose(fp);

                            break;
                        case 4:   //내 파일 업로드
                            send_pkt->option = 4;

                            char tmp_filename[20]="";
                            strcpy(tmp_filename, recv_pkt->msg);

                            printf("Uploading file... (%s)\n", tmp_filename);
                            FILE* fp4 = fopen(tmp_filename, "wb");

                            //파일 크기만큼만 읽어라-- 
                            while((read_cnt = read(ep_events[i].data.fd, recv_pkt, sizeof(*recv_pkt))) != 0){       //현재 서버로부터 받은 소켓, 읽어들이고 read
                                // read_cnt = read(ep_events[i].data.fd, recv_pkt, sizeof(*recv_pkt));
                                // if(read_cnt == 0) break;

                                printf(">>recv_pkt->msg: %.*s\n", recv_pkt->msg_read_cnt, recv_pkt->msg);
                                // printf(">>recv_pkt->file_eof: %d\n", recv_pkt->file_eof);
                                fwrite((void*)recv_pkt->msg, sizeof(char), recv_pkt->msg_read_cnt, fp4);    //buffer를 통해 fp(receive 파일로) fwrite
                                if(recv_pkt->file_eof == 1) break;
                            }
                            printf("file write 완료. \n");
                            fflush(stdout);
                            fclose(fp4);
                            memset(recv_pkt->msg, 0, sizeof(recv_pkt->msg));
                            memset(send_pkt->msg, 0, sizeof(send_pkt->msg));

                            snprintf(send_pkt->msg, sizeof(send_pkt->msg), "File Upload finished. (%s)\n", tmp_filename);
                            // printf(">>send_pkt->msg: %s\n", send_pkt->msg);
                            write(ep_events[i].data.fd, send_pkt, sizeof(*send_pkt));

                            break;
                        
                        default:
                            printf("Wrong Input. (%d)", recv_pkt->option);
                            break;
                    }
                    // printf("-------------------------------------------\n");
                    printf("send_pkt 전송 완료\n");
                    printf(">>option sent... (op: %d)\n", send_pkt->option);
                    // printf(">>send_pkt->msg: %s\n", send_pkt->msg);
                    // printf(">>send_pkt->msg_read_cnt: %d\n", send_pkt->msg_read_cnt);
                    // printf(">>send_pkt->file_eof: %d\n", send_pkt->file_eof);
                    // printf("===========================================\n");
                    fflush(stdout);

				}
			}
		}
	}
    free(send_pkt);
    free(recv_pkt);
	close(serv_sd);
	close(epfd);
	return 0;
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

int get_file_size(char* path){
    int size=0;
    FILE* fp = fopen(path, "r");
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    // printf("file: %s, size: %d\n", path, size);
    fclose(fp);
    return size;
}
