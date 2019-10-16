/*
 ============================================================================
 Name        : Socket.c
 Author      : LYW
 Version     : 1.1.0
 Copyright   : Your copyright notice
 Description : Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// 소켓 프로그래밍에 사용될 헤더파일 선언

#define BUF_LEN 128
// 메시지 송수신에 사용될 버퍼 크기를 선언
#define MAX_CLIENT 5
// 동시 접속 허용하는 최대 클라이언트 수 선언

int main(int argc, char *argv[])
{
    char buffer[BUF_LEN];
    struct sockaddr_in server_addr, client_addr;	// sockaddr_in : AF_INET(IPv4)에서 사용하는 구조체
    char temp[20];
    int server_fd, client_fd;
    //server_fd, client_fd : 각 소켓 번호
    int len, msg_size;

    // 실행 할 때 받은 파라미터가 없으면
    if(argc != 2)
    {
        printf("usage : %s [port]\n", argv[0]);
        exit(0);
    }

    // 소켓 생성, int형으로 소켓 번호가 생성되어 반환
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("Server : Can't open stream socket\n");
        exit(0);
    }
    memset(&server_addr, 0x00, sizeof(server_addr));	// server_Addr 을 NULL로 초기화
    server_addr.sin_family = AF_INET;					// IPv4 : AF_INET
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 	// INADDR_ANY 상수로 인해 자신의 IP값을 자동으로 할당
    server_addr.sin_port = htons(atoi(argv[1]));		// main함수의 파라미터로 받은 값인 argv[1]을 포트로 설정
    // server_addr 셋팅 // 소켓이 접속할 주소 지정

    // bind() 호출, 파라미터 (소켓번호, 서버의 소켓주소 구조체 포인터, 구조체의 크기) 성공시 0, 실패시 -1 리턴
    if(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <0)
    {
        printf("Server : Can't bind local address.\n");
        exit(0);
    }

    // 소켓을 수동 대기모드로 설정, 파라미터(소켓 번호, 연결 대기하는 클라이언트 최대 수)
    // 클라이언트가 listen()을 호출해 둔 서버 소켓을 목적지로 connect()를 호출
    // (여기서 3-way 핸드쉐이크가 발생한다. 연결확인) , 성공시 0, 실패시 -1
    // 시스템이 핸드쉐이크를 마친 후에는 서버 프로그램이 설정된 연결을 받아들이는 과정으로 accept()가 사용됨
    if(listen(server_fd, MAX_CLIENT) < 0)
    {
        printf("Server : Can't listening connect.\n");
        exit(0);
    }

    memset(buffer, 0x00, sizeof(buffer));	// 버퍼 초기화
    printf("Server : Wating connection request.\n");
    len = sizeof(client_addr);

    /* while문 시작 */
    while(1)
    {
    	// 연결된 클라이언트의 소켓주소 구조체와 소켓주소 구조체의 길이를 리턴
    	// accept()는 한번에 하나의 연결만 가능.
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);
        if(client_fd < 0)
        {
            printf("Server: accept failed.\n");
            exit(0);
        }

        //IPv4 와 IPv6 주소를 binary 형태에서 사람이 알아보기 쉬운 텍스트(human-readable text)형태로 전환해준다.
        inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, temp, sizeof(temp));

        printf("Server : %s client connected.\n", temp);

        // read함수로 1024바이트만큼 읽어서 buffer에 저장, 읽은 값의 길이를 반환
        // 만약 10바이트가 왔다면 1024바이트만큼 읽도록 하긴 했지만 실제로 온 10바이트만큼만 반환
        msg_size = read(client_fd, buffer, 1024);

        // 클라이언트에게 msg_size만큼 길이의 buffer 내용을 write
        write(client_fd, buffer, msg_size);

        close(client_fd);
        printf("Server : %s client closed.\n", temp);
    }
    /* while문 끝 */

    close(server_fd);
    return 0;
}
