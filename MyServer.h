#ifndef __MYRECTOR_H
#define __MYRECTOR_H

#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <semaphore.h>   //�ź���
#include <arpa/inet.h>
#include <errno.h>
#include <sstream>    //�ַ������������
#include <time.h>
#include <sys/stat.h>
#include <memory.h>
#include <signal.h>
#include <string.h>
#include <list>
//#include <unordered_map>
#include <queue>
#include <map>
#include <set>
#include <stdlib.h>

#include "Command.h"
#include <mysql.h>

#define OPEN_MAX 1024
#define WORKER_NUMBER 5
#define GROUP_NUMBER 5

class MyRector{

public:
	MyRector();
	~MyRector();

	bool Init(char *IP, int port);
	bool Uninit();
	static bool main_loop(MyRector *rec);
	bool close_client(int cfd);
	bool create_server_listener(char *IP, int port);
	static void* accept_thread_proc(void *rec);
	static void* woker_thread_proc(void *rec);
	static void* send_thread_proc(void *rec);
	static void* group_thread_proc(void *rec);


	//�������ݿ�
	int connect();
	//���ݿ��ѯ
	int Mysql_query(const char *);   



private:

	//�߳�
	pthread_t my_accept_id;
	pthread_t my_send_id;    //���ڷ��͵��߳�
	pthread_t my_thread_ids[WORKER_NUMBER];
	pthread_t my_group_thread_ids[GROUP_NUMBER];

	int lfd, cfd;
	int efd;   // ���������������
	struct epoll_event temps[OPEN_MAX];    //��������������Ŀͻ���

	//������Դ----�ٽ���Դ
	std::list<int> waiting_for_service;  //����ȴ�����Ŀͻ���
	std::map<int, std::string> all_fds;               //���������Ѿ����ӵĿͻ���
	std::deque<std::string>  msg_que;    //��Ϣ����

	/*std::map<std::string, std::map<string, string>> *group1;      //Ⱥ��������Ϣ����
	std::map<std::string, std::string> *group1_msg;

	std::map<std::string, std::map<string, string>> *group2;
	std::map<std::string, std::string> *group2_msg;

	std::map<std::string, std::map<string, string>> *group3;
	std::map<std::string, std::string> *group3_msg;

	std::map<std::string, std::map<string, string>> *group4;
	std::map<std::string, std::string> *group4_msg;

	std::map<std::string, std::map<string, string>> *group5;
	std::map<std::string, std::string> *group5_msg;*/

	std::list<std::map<std::string, std::map<std::string, std::string>*>* > group_msg_list;

	bool Stop;  //�ܿ���

	//������������

	//�������ӵ���
	pthread_mutex_t my_accept_mutex = PTHREAD_MUTEX_INITIALIZER;

	//������������������----waiting_for_service
	pthread_mutex_t my_client_mutex = PTHREAD_MUTEX_INITIALIZER;

	//������������ѹ�����Ӽ��ϵ����� ��------all_fds
	pthread_mutex_t my_cli_accept_mutex = PTHREAD_MUTEX_INITIALIZER;

	//������Ϣ������------msg_que
	pthread_mutex_t m_send_mutex = PTHREAD_MUTEX_INITIALIZER;

	//Ⱥ����Ϣmap  list��
	pthread_mutex_t m_send_group_mutex = PTHREAD_MUTEX_INITIALIZER;

	//Ⱥ�ķ�������������
	pthread_cond_t m_send_group_cond = PTHREAD_COND_INITIALIZER;

	//�������ӵ���������
	pthread_cond_t my_accept_cond = PTHREAD_COND_INITIALIZER;

	//������������
	pthread_cond_t my_cli_cond = PTHREAD_COND_INITIALIZER;

	pthread_cond_t my_client_cond = PTHREAD_COND_INITIALIZER;


	//������������
	pthread_cond_t my_send_cond = PTHREAD_COND_INITIALIZER;

	//���ݿ����
	MYSQL *mysql;
	MYSQL_RES *mysql_res;
	MYSQL_ROW mysql_row;
};



#endif
