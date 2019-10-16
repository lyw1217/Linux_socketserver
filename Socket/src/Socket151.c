/*
 ============================================================================
 Name        : Socket.c
 Author      : LYW
 Version     : 1.5.1
 Copyright   : Your copyright notice
 Description : ��Ƽ�÷��� �����ϰԲ� epoll ��� / ���ڿ� split��� �߰�
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

#define BUF_SIZE 		256
#define EPOLL_SIZE 		50
#define MAX_CLIENT 		20
#define IP_ADDR_SIZE	20

// �Լ� �⺻�� ����
void error_handling(char *buf);
void print_time();
void print_log(char *buff);
int write_logfile( char *buff );
char** split_List(char *str);

// �������� ����
char log_buff[BUF_SIZE] = { 0x00, };	// �α׸� ���� ����
volatile time_t rawtime; // �ð� ����� ���� �������� ����
volatile struct tm tm;

// ���� ��� ����ü ����
struct member{
	char* id;
	char* pw;
};

int main(int argc, char *argv[]) {
    int server_sock, client_sock;	// �� ���� ��ȣ
    struct sockaddr_in server_addr, client_addr; // sockaddr_in : AF_INET(IPv4)���� ����ϴ� ����ü
    socklen_t addr_size;
    int str_len, i;
    char buf[BUF_SIZE];
	char addr_Temp[IP_ADDR_SIZE];	// ip �ּ� ����� ���� Temp
	char log_Temp[256]; // log ������ ���� temp
	char **splitLists;

	// ���� ��� ����ü �迭 �ʱ�ȭ
	struct member User[MAX_CLIENT] = { {NULL, NULL},};
	int user_count = 0;
	
	// �ð� ����� ���� �������� �ʱ�ȭ
	rawtime = time(NULL);
	tm = *localtime(&rawtime);

    struct epoll_event *ep_events;
    struct epoll_event event;
    int epfd, event_cnt;

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

    /*�α�ó����ƾ*/
    memset(log_Temp, 0x00, sizeof(log_Temp)); // �����ʱ�ȭ
    sprintf(log_Temp, "Server Start : Waiting Connection Request.\n");
    print_log(log_Temp);

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

				/*�α�ó����ƾ*/
				memset(log_Temp, 0x00, sizeof(log_Temp)); // �����ʱ�ȭ
				sprintf(log_Temp,"Server : Connect Request %d, IP : %s \n", client_sock, addr_Temp);
				print_log(log_Temp);
            } else {
				// read�Լ��� BUF_SIZE��ŭ �о buf�� ����, ���� ���� ���̸� ��ȯ
				// ���� 10����Ʈ�� �Դٸ� BUF_SIZE��ŭ �е��� �ϱ� ������ ������ �� 10����Ʈ��ŭ�� ��ȯ
            	memset(buf, 0x00, sizeof(buf));
                str_len = read(ep_events[i].data.fd, buf, BUF_SIZE);

                if (str_len == 0) { // close request!
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);
                    close(ep_events[i].data.fd);

                    /*�α�ó����ƾ*/
                    memset(log_Temp, 0x00, sizeof(log_Temp)); // �����ʱ�ȭ
                    sprintf(log_Temp,"Server : Closed client %d, IP : %s\n", ep_events[i].data.fd, addr_Temp);
                    print_log(log_Temp);
                } else {
                    /*�α�ó����ƾ*/
                    memset(log_Temp, 0x00, sizeof(log_Temp)); // �����ʱ�ȭ
                    sprintf(log_Temp,"Server : Client %d send : %s\n",ep_events[i].data.fd, buf);
                    print_log(log_Temp);

                    /*���ڿ� ó����*/
                    splitLists = split_List(buf);
                    // if received msg = [ALLMSG]abc@123
                    // splitLists[0] : ALLMSG | splitLists[1] : abc | splitLists[2] : 123
                    if(!strcmp(splitLists[1], "login")){
                    	// �α��� ��û Ȯ��, ���
                    	// ���� DB ����
                    	User[user_count].id = splitLists[0];
                    	User[user_count].pw = splitLists[2];
                    	user_count++;
                    	if(user_count >= MAX_CLIENT){
                    		user_count = 0;
                    	}
                    }else{
                    	// �α��� ��û�� �ƴ϶�� [ID]�� �����ϴ��� Ȯ��
                    	for(int num = 0; num < MAX_CLIENT; num++){
                    		if(!strcmp(splitLists[0], User[i].id)){
                    			// �α��� ��û�� id�� ���ϵ� id�� ��ġ�Ѵٸ�
                    			
                    			break;
                    		}else{
                    			// ��ϵ� id�� ���ٸ� fail msg �۽�, ��� �� break;
                    			/*memset(log_Temp, 0x00, sizeof(log_Temp)); // �����ʱ�ȭ
                    			sprintf(log_Temp, "Incorrect ID, Try again\n");
                    			print_log(log_Temp);
                    			write(ep_events[i].data.fd, log_Temp, sizeof(log_Temp));*/
                    			break;
                    		}
                    	}
                    }
                    
                    write(ep_events[i].data.fd, buf, str_len);    // echo!
                    
                    printf("splitLists[0] : %s\n", splitLists[0]);
                    printf("splitLists[1] : %s\n", splitLists[1]);
                    printf("splitLists[2] : %s\n", splitLists[2]);
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

void print_log(char *buff){
	// �α� ó�� ��ƾ
	print_time();
	printf(buff);
	
	memset(log_buff, 0x00, sizeof(log_buff)); // �����ʱ�ȭ
	sprintf(log_buff, "%02d-%02d-%02d %02d:%02d:%02d %s",tm.tm_year-100, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, buff); // �޼��� ����
	write_logfile( log_buff ); //�����Լ� ȣ��
}

// �α� ���� �Լ�
int write_logfile( char *buff )
{
    int file_access;
   
    FILE *fd_log_file;

    char log_file[32] = {0,};
    sprintf(log_file,"./%02d_%02d_%02d_socket.log", tm.tm_year-100, tm.tm_mon+1, tm.tm_mday);
   
    file_access = access(log_file, R_OK | W_OK);
   
    if ( file_access == 0 ){
       fd_log_file = fopen(log_file, "a+");
    }else{  
        fd_log_file = fopen(log_file, "w+");
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

char** split_List(char *str){
	char delimeter[4] = "[:]@";
	char *token;
	static char *buff[10] = {NULL, };
	int i = 0;
	token = strtok(str, delimeter);
	while(token){
		buff[i] = token;
		token = strtok(NULL, delimeter);
		i++;
	}
	return buff;
}
