/*
 ============================================================================
 Name        : Socket.c
 Author      : LYW
 Version     : 1.3.0
 Copyright   : Your copyright notice
 Description : ��Ƽ�÷��� �����ϰԲ� epoll ��� / �α� ��� �߰�
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h> // ������������ ��� ����
#include <time.h>
#include <stdarg.h> // �������� ����� ���� ���

#define BUF_SIZE 100
#define EPOLL_SIZE 50
#define MAX_CLIENT 5

#define LOG_FILE	"./log/socket.log"// �α� ���� ��� ����

// �Լ� �⺻�� ����
void error_handling(char *buf);
void print_time();
void print_log( char *buf);
int write_logfile( char *buff );

// �������� ����
char log_buff[256] = { 0x00, };	// �α׸� ���� ����
volatile time_t rawtime = time(NULL);
volatile struct tm tm = *localtime(&rawtime);

int main(int argc, char *argv[]) {
    int server_sock, client_sock;	// �� ���� ��ȣ
    struct sockaddr_in server_addr, client_addr; // sockaddr_in : AF_INET(IPv4)���� ����ϴ� ����ü
    socklen_t addr_size;
    int str_len, i;
    char buf[BUF_SIZE];
	char addr_Temp[20];	// ip �ּ� ����� ���� Temp
	char log_Temp[256]; // log ������ ���� temp


    struct epoll_event *ep_events;
    struct epoll_event event;
    int epfd, event_cnt;

    int logfd;

    logfd = open("./");

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

	// ���� ����, int������ ���� ��ȣ�� �����Ǿ� ��ȯ
    if((server_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		error_handling("Server : Can't open stream socket");

    memset(&server_addr, 0, sizeof(server_addr));	// server_Addr �� NULL�� �ʱ�ȭ
    server_addr.sin_family = AF_INET;				// IPv4 : AF_INET
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);// INADDR_ANY ����� ���� �ڽ��� IP���� �ڵ����� �Ҵ�
    server_addr.sin_port = htons(atoi(argv[1]));	// main�Լ��� �Ķ���ͷ� ���� ���� argv[1]�� ��Ʈ�� ����

	// bind() ȣ��, �Ķ���� (���Ϲ�ȣ, ������ �����ּ� ����ü ������, ����ü�� ũ��) ������ 0, ���н� -1 ����
    if (bind(server_sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1){
        close(server_sock);
        error_handling("Server : bind() error");
    }

	// ������ ���� ������ ����, �Ķ����(���� ��ȣ, ���� ����ϴ� Ŭ���̾�Ʈ �ִ� ��)
	// Ŭ���̾�Ʈ�� listen()�� ȣ���� �� ���� ������ �������� connect()�� ȣ��
    // (���⼭ 3-way �ڵ彦��ũ�� �߻��Ѵ�. ����Ȯ��) , ������ 0, ���н� -1
    // �ý����� �ڵ彦��ũ�� ��ģ �Ŀ��� ���� ���α׷��� ������ ������ �޾Ƶ��̴� �������� accept()�� ����
	if (listen(server_sock, MAX_CLIENT) == -1){
        close(server_sock);
        error_handling("Server : listen() error");

	}

    // Ŀ���� �����ϴ� epoll �ν��Ͻ��� �Ҹ��� ���� ��ũ������ ����� ����
    // ���� �� epoll ���� ��ũ����, ���н� -1 ��ȯ
    if((epfd = epoll_create(EPOLL_SIZE)) == -1)
		error_handling("Server : epoll_create() error");

    if((ep_events = malloc(sizeof(struct epoll_event)*EPOLL_SIZE)) == NULL){
    	close(server_sock);
    	close(epfd);
    	error_handling("Server : malloc() error");
    }


    event.events = EPOLLIN;
    event.data.fd = server_sock;
    // ���� ��ũ����(server_sock)�� epoll �ν��Ͻ��� ����Ѵ�. (��������� ���� �̺�Ʈ ������ EPOLLIN)
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_sock, &event);

    print_time();
    print_log("Server : Waiting Connection Request.\n");

	/* while�� ���� */
    while(1) {
        // ���� �� �̺�Ʈ�� �߻��� ���� ��ũ������ ��, ���� �� -1 ��ȯ
        // �� ��° ���ڷ� ���޵� �ּ��� �޸� ������ �̺�Ʈ �߻��� ���� ��ũ���Ϳ� ���� ������ ����ִ�.
        event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
        if (event_cnt == -1) {
            puts("Server : epoll_wait() error");
            break;
        }

        for (i = 0; i < event_cnt; i++) {
            if (ep_events[i].data.fd == server_sock) {
                addr_size = sizeof(client_addr);

				// ����� Ŭ���̾�Ʈ�� �����ּ� ����ü�� �����ּ� ����ü�� ���̸� ����
                client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
                event.events = EPOLLIN;
                event.data.fd = client_sock;
                // ���� ��ũ����(client_sock)�� epoll �ν��Ͻ��� ����Ѵ�. (��������� ���� �̺�Ʈ ������ EPOLLIN)
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_sock, &event);

				// IPv4 �� IPv6 �ּҸ� binary ���¿��� ����� �˾ƺ��� ���� �ؽ�Ʈ(human-readable text)���·� ��ȯ���ش�.
				inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, addr_Temp, sizeof(addr_Temp));
				print_time();
                printf("Server : Connected client %d, IP : %s \n", client_sock, addr_Temp);
            } else {
				// read�Լ��� BUF_SIZE��ŭ �о buf�� ����, ���� ���� ���̸� ��ȯ
				// ���� 10����Ʈ�� �Դٸ� BUF_SIZE��ŭ �е��� �ϱ� ������ ������ �� 10����Ʈ��ŭ�� ��ȯ
                str_len = read(ep_events[i].data.fd, buf, BUF_SIZE);
                if (!strcmp(buf, "Q\n") || (str_len == 0)) { // close request!
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);
                    close(ep_events[i].data.fd);
                    print_time();
                    printf("Server : Closed client %d, IP : %s\n", ep_events[i].data.fd, addr_Temp);
                } else {
                	print_time();
                	printf("Server : Client %d send : %s\n",ep_events[i].data.fd, buf);
                    write(ep_events[i].data.fd, buf, str_len);    // echo!
                }
            }
        }
    }
	/* while�� �� */

    close(server_sock);
    close(epfd);
    return 0;
}

void error_handling(char *buf) {
    fputs(buf, stderr);
    fputc('\n', stderr);

    exit(1);
}

void print_time(){
	// �������� ó��
	rawtime = time(NULL);
	tm = *localtime(&rawtime);

   	printf("%02d-%02d-%02d %02d:%02d:%02d ", tm.tm_year-100, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void print_log( char *buf){
	printf(buf);

	// �α� ó�� ��ƾ
	// �����ʱ�ȭ
	memset(log_buff, 0x00, sizeof(log_buff));
	// �޼��� ����
	sprintf(log_buff, "%02d-%02d-%02d %02d:%02d:%02d %s",tm.tm_year-100, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, buf);
	//�����Լ� ȣ��
	write_logfile( log_buff );
}

// �α� ���� �Լ�
int write_logfile( char *buff )
{
    int file_access;

    FILE *fd_log_file;

    file_access = access(LOG_FILE, R_OK | W_OK);

    if ( file_access == 0 ){
       fd_log_file = fopen(LOG_FILE, "a+");
    }else{
        fd_log_file = fopen(LOG_FILE, "w+");
    }

    if ( fd_log_file == NULL ){
        printf("log file open error\n");
        return -1;
    }

    fseek( fd_log_file, 0, SEEK_END);

    printf("%s", buff);
    fwrite(buff, 1, strlen(buff), fd_log_file);
    fclose(fd_log_file);

    return 1;
}
