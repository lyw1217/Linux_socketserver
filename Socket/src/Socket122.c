/*
 ============================================================================
 Name        : Socket.c
 Author      : LYW
 Version     : 1.2.2
 Copyright   : Your copyright notice
 Description : 멀티플렉싱 가능하게끔 epoll 사용
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h> // 리눅스에서만 사용 가능

#define BUF_SIZE 100
#define EPOLL_SIZE 50
#define MAX_CLIENT 5

void error_handling(char *buf);

int main(int argc, char *argv[]) {
    int server_sock, client_sock;	// 각 소켓 번호
    struct sockaddr_in server_addr, client_addr; // sockaddr_in : AF_INET(IPv4)에서 사용하는 구조체
    socklen_t addr_size;
    int str_len, i;
    char buf[BUF_SIZE];
	char addr_Temp[20];

    struct epoll_event *ep_events;
    struct epoll_event event;
    int epfd, event_cnt;

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

	// 소켓 생성, int형으로 소켓 번호가 생성되어 반환
    if((server_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		error_handling("Server : Can't open stream socket");

    memset(&server_addr, 0, sizeof(server_addr));	// server_Addr 을 NULL로 초기화
    server_addr.sin_family = AF_INET;				// IPv4 : AF_INET
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);// INADDR_ANY 상수로 인해 자신의 IP값을 자동으로 할당
    server_addr.sin_port = htons(atoi(argv[1]));	// main함수의 파라미터로 받은 값인 argv[1]을 포트로 설정

	// bind() 호출, 파라미터 (소켓번호, 서버의 소켓주소 구조체 포인터, 구조체의 크기) 성공시 0, 실패시 -1 리턴
    if (bind(server_sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1){
        close(server_sock);
        error_handling("Server : bind() error");
    }

	// 소켓을 수동 대기모드로 설정, 파라미터(소켓 번호, 연결 대기하는 클라이언트 최대 수)
	// 클라이언트가 listen()을 호출해 둔 서버 소켓을 목적지로 connect()를 호출
    // (여기서 3-way 핸드쉐이크가 발생한다. 연결확인) , 성공시 0, 실패시 -1
    // 시스템이 핸드쉐이크를 마친 후에는 서버 프로그램이 설정된 연결을 받아들이는 과정으로 accept()가 사용됨
	if (listen(server_sock, MAX_CLIENT) == -1){
        close(server_sock);
        error_handling("Server : listen() error");

	}

    // 커널이 관리하는 epoll 인스턴스라 불리는 파일 디스크립터의 저장소 생성
    // 성공 시 epoll 파일 디스크립터, 실패시 -1 반환
    if((epfd = epoll_create(EPOLL_SIZE)) == -1)
		error_handling("Server : epoll_create() error");

    if((ep_events = malloc(sizeof(struct epoll_event)*EPOLL_SIZE)) == NULL){
    	close(server_sock);
    	close(epfd);
    	error_handling("Server : malloc() error");
    }


    event.events = EPOLLIN;
    event.data.fd = server_sock;
    // 파일 디스크립터(server_sock)를 epoll 인스턴스에 등록한다. (관찰대상의 관찰 이벤트 유형은 EPOLLIN)
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_sock, &event);

    printf("Server : Waiting Connection Request.\n");

	/* while문 시작 */
    while(1) {
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
                client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
                event.events = EPOLLIN;
                event.data.fd = client_sock;
                // 파일 디스크립터(client_sock)를 epoll 인스턴스에 등록한다. (관찰대상의 관찰 이벤트 유형은 EPOLLIN)
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_sock, &event);

				//IPv4 와 IPv6 주소를 binary 형태에서 사람이 알아보기 쉬운 텍스트(human-readable text)형태로 전환해준다.
				inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, addr_Temp, sizeof(addr_Temp));
                printf("Server : connected client: %d, %s \n", client_sock, addr_Temp);
            } else {
				// read함수로 BUF_SIZE만큼 읽어서 buf에 저장, 읽은 값의 길이를 반환
				// 만약 10바이트가 왔다면 BUF_SIZE만큼 읽도록 하긴 했지만 실제로 온 10바이트만큼만 반환
                str_len = read(ep_events[i].data.fd, buf, BUF_SIZE);
                if (!strcmp(buf, "Q\n") || (str_len == 0)) { // close request!
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);
                    close(ep_events[i].data.fd);
                    printf("Server : closed client: %d, %s \n", ep_events[i].data.fd, addr_Temp);
                } else {
                    write(ep_events[i].data.fd, buf, str_len);    // echo!
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
