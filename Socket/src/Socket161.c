/*
 ============================================================================
 Name        : Socket.c
 Author      : LYW
 Version     : 1.6.1
 Copyright   : Your copyright notice
 Description : 채팅 기능 구현
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h> // 리눅스에서만 사용 가능
#include <time.h>

#define DEBUG			0

#define BUF_SIZE 		256
#define EPOLL_SIZE 		50
#define MAX_CLIENT 		20
#define IP_ADDR_SIZE	20
#define MAX_USER_LEN	20
// 함수 기본형 선언
void error_handling(char *buf);
void print_time();
void print_log(char *buff);
int write_logfile(char *buff);

// 전역변수 선언
char log_buff[BUF_SIZE] = { 0x00, };	// 로그를 위한 버퍼
volatile time_t rawtime; // 시간 출력을 위한 전역변수 선언
volatile struct tm tm;

// 유저 멤버 구조체 선언
struct member {
	char id[MAX_USER_LEN];
	char pw[MAX_USER_LEN];
};

int main(int argc, char *argv[]) {
	int server_sock, client_sock;	// 각 소켓 번호
	struct sockaddr_in server_addr, client_addr; // sockaddr_in : AF_INET(IPv4)에서 사용하는 구조체
	socklen_t addr_size;
	int str_len, i, num, log_Temp_len;
	char buf[BUF_SIZE];
	char split_buf[BUF_SIZE];
	char addr_Temp[IP_ADDR_SIZE];	// ip 주소 출력을 위한 Temp
	char log_Temp[256]; // log 저장을 위한 temp
	char *splitLists[10];	// split을 위한 포인터 배열

	char delimeter[4] = "[]@";
	char *token;
	int split_count = 0;

	int client_fd[MAX_CLIENT];
	int client_index = 0;

	// 유저 멤버 구조체 배열 선언, 초기화
	struct member User[MAX_CLIENT] = { { NULL, NULL }, };

	int user_count = 0;
	int signup_flag = 0;

	// 시간 출력을 위한 전역변수 초기화
	rawtime = time(NULL);
	tm = *localtime(&rawtime);

	struct epoll_event *ep_events;
	struct epoll_event event;
	int epfd, event_cnt;

	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	// 소켓 생성, int형으로 소켓 번호가 생성되어 반환
	if ((server_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		error_handling("Server : Can't open stream socket");

	memset(&server_addr, 0, sizeof(server_addr));	// server_Addr 을 NULL로 초기화
	server_addr.sin_family = AF_INET;				// IPv4 : AF_INET
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);// INADDR_ANY 상수로 인해 자신의 IP값을 자동으로 할당
	server_addr.sin_port = htons(atoi(argv[1]));// main함수의 파라미터로 받은 값인 argv[1]을 포트로 설정

	// bind() 호출, 파라미터 (소켓번호, 서버의 소켓주소 구조체 포인터, 구조체의 크기) 성공시 0, 실패시 -1 리턴
	if (bind(server_sock, (struct sockaddr*) &server_addr, sizeof(server_addr))
			== -1) {
		close(server_sock);
		error_handling("Server : bind() error");
	}

	// 소켓을 수동 대기모드로 설정, 파라미터(소켓 번호, 연결 대기하는 클라이언트 최대 수)
	// 클라이언트가 listen()을 호출해 둔 서버 소켓을 목적지로 connect()를 호출
	// (여기서 3-way 핸드쉐이크가 발생한다. 연결확인) , 성공시 0, 실패시 -1
	// 시스템이 핸드쉐이크를 마친 후에는 서버 프로그램이 설정된 연결을 받아들이는 과정으로 accept()가 사용됨
	if (listen(server_sock, MAX_CLIENT) == -1) {
		close(server_sock);
		error_handling("Server : listen() error");

	}

	// 커널이 관리하는 epoll 인스턴스라 불리는 파일 디스크립터의 저장소 생성
	// 성공 시 epoll 파일 디스크립터, 실패시 -1 반환
	if ((epfd = epoll_create(EPOLL_SIZE)) == -1)
		error_handling("Server : epoll_create() error");

	if ((ep_events = malloc(sizeof(struct epoll_event) * EPOLL_SIZE)) == NULL) {
		close(server_sock);
		close(epfd);
		error_handling("Server : malloc() error");
	}

	event.events = EPOLLIN;
	event.data.fd = server_sock;
	// 파일 디스크립터(server_sock)를 epoll 인스턴스에 등록한다. (관찰대상의 관찰 이벤트 유형은 EPOLLIN)
	epoll_ctl(epfd, EPOLL_CTL_ADD, server_sock, &event);

	/*로그처리루틴*/
	memset(log_Temp, 0x00, sizeof(log_Temp)); // 버퍼초기화
	sprintf(log_Temp, "Server Start : Waiting Connection Request.");
	print_log(log_Temp);

	/* while문 시작 */
	while (1) {
		// 성공 시 이벤트가 발생한 파일 디스크립터의 수, 실패 시 -1 반환
		// 두 번째 인자로 전달된 주소의 메모리 공간에 이벤트 발생한 파일 디스크립터에 대한 정보가 들어있다.
		event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
		if (event_cnt == -1) {
			puts("Server : epoll_wait() error");
			break;
		}

		for (i = 0; i < event_cnt; i++) {
			if (ep_events[i].data.fd == server_sock) {
				addr_size = sizeof(client_addr);

				// 연결된 클라이언트의 소켓주소 구조체와 소켓주소 구조체의 길이를 리턴
				client_sock = accept(server_sock,
						(struct sockaddr*) &client_addr, &addr_size);
				event.events = EPOLLIN;
				event.data.fd = client_sock;
				// 파일 디스크립터(client_sock)를 epoll 인스턴스에 등록한다. (관찰대상의 관찰 이벤트 유형은 EPOLLIN)
				epoll_ctl(epfd, EPOLL_CTL_ADD, client_sock, &event);

				// IPv4 와 IPv6 주소를 binary 형태에서 사람이 알아보기 쉬운 텍스트(human-readable text)형태로 전환해준다.
				inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, addr_Temp,
						sizeof(addr_Temp));

				/*로그처리루틴*/
				memset(log_Temp, 0x00, sizeof(log_Temp)); // 버퍼초기화
				sprintf(log_Temp, "Server : Connect Request %d, IP : %s ",
						client_sock, addr_Temp);
				print_log(log_Temp);

				// 클라이언트 파일디스크립터 번호 저장
				client_fd[client_index++] = client_sock;
				if(client_index >= MAX_CLIENT) client_index = 0;
#ifdef DEBUG
			printf("event_cnt : %d\n", event_cnt);
			printf("ep_events[i].data.fd : %d\n", ep_events[i].data.fd);
			printf("server_sock : %d\n", server_sock);
			printf("client_sock : %d\n", client_sock);
#endif
			} else {
				// read함수로 BUF_SIZE만큼 읽어서 buf에 저장, 읽은 값의 길이를 반환
				// 만약 10바이트가 왔다면 BUF_SIZE만큼 읽도록 하긴 했지만 실제로 온 10바이트만큼만 반환
				memset(buf, 0x00, sizeof(buf));
				str_len = read(ep_events[i].data.fd, buf, BUF_SIZE);

				if (str_len == 0) { // close request!
					epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);
					close(ep_events[i].data.fd);

					/*로그처리루틴*/
					memset(log_Temp, 0x00, sizeof(log_Temp)); // 버퍼초기화
					sprintf(log_Temp, "Server : Closed client %d, IP : %s",
							ep_events[i].data.fd, addr_Temp);
					print_log(log_Temp);

					// 클라이언트 파일디스크립터 번호 회수
					client_index--;
				} else {
					/*로그처리루틴*/
					memset(log_Temp, 0x00, sizeof(log_Temp)); // 버퍼초기화
					sprintf(log_Temp, "Server : Client %d send : %s",
							ep_events[i].data.fd, buf);
					print_log(log_Temp);

					/*문자열 처리부 [split]*/
					memset(split_buf, 0x00, sizeof(split_buf));
					memset(splitLists, 0x00, sizeof(splitLists));
					strcpy(split_buf, buf);
					split_count = 0;
					token = strtok(split_buf, delimeter);
					while (token) {
						splitLists[split_count] = token;
						token = strtok(NULL, delimeter);
						split_count++;
					}

					// if received msg = [ALLMSG]abc@123
					// splitLists[0] : ALLMSG | splitLists[1] : abc | splitLists[2] : 123
#ifdef DEBUG
					printf(
							"splitLists[0] : %s\nsplitLists[1] : %s\nsplitLists[2] : %s\n",
							splitLists[0], splitLists[1], splitLists[2]);
					for (num = 0; num < MAX_CLIENT; num++) {
						printf("user '%d' id %s / pw %s\n", num, User[num].id,
								User[num].pw);
					}
					printf("1. user_count = %d\n", user_count);
#endif
					if (splitLists[0] != NULL) {
						if (!strcmp(splitLists[0], "ALLMSG")) {
							/*로그처리루틴*/
							memset(log_Temp, 0x00, sizeof(log_Temp)); // 버퍼초기화
							sprintf(log_Temp, "%s", buf);
							log_Temp_len = strlen(log_Temp);
							print_log(log_Temp);
							for (num = 0; num < client_index; num++) {
								if (write(num, log_Temp, log_Temp_len) != log_Temp_len) {
									error_handling("write() Error!");
#ifdef DEBUG
									printf("write() Error!\n");
#endif
								}
							}
						} else {
							if (splitLists[1] != NULL) {
								///////////////////////////////////////////////////////////
								//*********************** 문자열 비교부 **********************//
								///////////////////////////////////////////////////////////
								if (!strcmp(splitLists[1], "sign")) {
									// [ID]sign@PW 형식으로 오면
									// 추후 DB 연동
									// 중복 ID 확인
									for (num = 0; num < MAX_CLIENT; num++) {
#ifdef DEBUG
										printf(
												"[sign] user '%d' id %s / pw %s\n",
												num, User[num].id,
												User[num].pw);
#endif
										if (User[num].id != NULL) {
											// 예외처리
											if (!strcmp(splitLists[0],
													User[num].id)) {
												// 중복된 id가 있다면
												/*로그처리루틴*/
												memset(log_Temp, 0x00,
														sizeof(log_Temp)); // 버퍼초기화
												sprintf(log_Temp,
														"Server : %s is already exist",
														splitLists[0]);
												log_Temp_len = strlen(log_Temp);
												if (write(ep_events[i].data.fd,
														log_Temp, log_Temp_len)
														!= log_Temp_len) {
													error_handling(
															"write() Error!");
												}
												print_log(log_Temp);
												break;
											}
										}
										// 마지막까지 중복된 ID가 없다면 플래그
										if (num >= (MAX_CLIENT - 1)) {
											signup_flag = 1;
										}
									}
									if (signup_flag) {
										signup_flag = 0;
										// 중복된 id 없으면 구조체에 등록
										// 만약 pw가 안오면? splitLists[2] 가 없다면 어떡하지
										for (num = 0;
												num < strlen(splitLists[0]);
												num++) {
											User[user_count].id[num] =
													splitLists[0][num];
										}
										for (num = 0;
												num < strlen(splitLists[2]);
												num++) {
											User[user_count].pw[num] =
													splitLists[2][num];
										}

										/*로그처리루틴*/
										memset(log_Temp, 0x00,
												sizeof(log_Temp)); // 버퍼초기화
										sprintf(log_Temp,
												"Server : Client '%s' sign up!",
												User[user_count].id);
										log_Temp_len = strlen(log_Temp);
										if (write(ep_events[i].data.fd,
												log_Temp, log_Temp_len)
												!= log_Temp_len) {
											error_handling("write() Error!");
										}
										print_log(log_Temp);

										user_count++;
										if (user_count >= MAX_CLIENT) {
											user_count = 0;
										}
#ifdef DEBUG
										printf("2. user_count = %d\n",
												user_count);
#endif
									}
								} else if (!strcmp(splitLists[1], "Q")) {
									/*로그처리루틴*/
									memset(log_Temp, 0x00, sizeof(log_Temp)); // 버퍼초기화
									sprintf(log_Temp, "Server : Close Server.");
									log_Temp_len = strlen(log_Temp);
									if (write(ep_events[i].data.fd, log_Temp,
											log_Temp_len) != log_Temp_len) {
										error_handling("write() Error!");
									}
									print_log(log_Temp);
									close(server_sock);
									close(epfd);
									return 0;
								} else if (!strcmp(splitLists[1], "IDLIST")) {

								} else {
									///////////// 문자열 비교부 끝 /////////////

									// sign형식이 아니라면 [ID]가 존재하는지 확인
									for (num = 0; num < MAX_CLIENT; num++) {
										if (User[num].id != NULL) {
											// 예외처리
											if (!strcmp(splitLists[0],
													User[num].id)) {
												// 로그인 요청한 id와 기등록된 id가 일치한다면
#ifdef DEBUG
												printf(
														"[not sign] user '%d' id %s / pw %s\n",
														num, User[num].id,
														User[num].pw);
												printf("user_count = %d\n",
														user_count);
#endif
												write(ep_events[i].data.fd, buf,
														str_len);    // echo!
												break;
											}
										}

										if (num == (MAX_CLIENT - 1)) {
											// 등록된 id가 없다면 fail msg 송신, 출력 후 break;
											memset(log_Temp, 0x00,
													sizeof(log_Temp)); // 버퍼초기화
											sprintf(log_Temp,
													"Incorrect ID, Try again");
											print_log(log_Temp);
											log_Temp_len = strlen(log_Temp);
											if (write(ep_events[i].data.fd,
													log_Temp, log_Temp_len)
													!= log_Temp_len) {
												error_handling(
														"write() Error!");
											}
										}
									}
								}
							} else {
								//splitLists[1] == NULL 이라면
								memset(log_Temp, 0x00, sizeof(log_Temp)); // 버퍼초기화
								sprintf(log_Temp,
										"Incorrect Message, Try again");
								print_log(log_Temp);
								log_Temp_len = strlen(log_Temp);
								if (write(ep_events[i].data.fd, log_Temp,
										log_Temp_len) != log_Temp_len) {
									error_handling("write() Error!");
								}
							}
						}
					}
				}
			}
		}
	}
	/* while문 끝 */

	close(server_sock);
	close(epfd);
	return 0;
}

void error_handling(char *buf) {
	fputs(buf, stderr);
	fputc('\n', stderr);

	exit(1);
}

void print_time() {
	// 전역변수 처리
	rawtime = time(NULL);
	tm = *localtime(&rawtime);

	printf("%02d-%02d-%02d %02d:%02d:%02d ", tm.tm_year - 100, tm.tm_mon + 1,
			tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void print_log(char *buff) {
	// 로그 처리 루틴
	print_time();
	printf(buff);
	printf("\n");

	memset(log_buff, 0x00, sizeof(log_buff)); // 버퍼초기화
	sprintf(log_buff, "%02d-%02d-%02d %02d:%02d:%02d %s\n", tm.tm_year - 100,
			tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, buff); // 메세지 저장
	write_logfile(log_buff); //저장함수 호출
}

// 로그 저장 함수
int write_logfile(char *buff) {
	int file_access;

	FILE *fd_log_file;

	char log_file[32] = { 0, };
	sprintf(log_file, "./%02d_%02d_%02d_socket.log", tm.tm_year - 100,
			tm.tm_mon + 1, tm.tm_mday);

	file_access = access(log_file, R_OK | W_OK);

	if (file_access == 0) {
		fd_log_file = fopen(log_file, "a+");
	} else {
		fd_log_file = fopen(log_file, "w+");
	}

	if (fd_log_file == NULL) {
		printf("log file open error\n");
		return -1;
	}

	fseek(fd_log_file, 0, SEEK_END);

	fwrite(buff, 1, strlen(buff), fd_log_file);
	fclose(fd_log_file);

	return 1;
}
