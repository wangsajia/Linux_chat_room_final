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
#include <semaphore.h>   //信号量
#include <arpa/inet.h>
#include <errno.h>
#include <sstream>    //字符串输入输出流
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


	//连接数据库
	int connect();
	//数据库查询
	int Mysql_query(const char *);   



private:

	//线程
	pthread_t my_accept_id;
	pthread_t my_send_id;    //用于发送的线程
	pthread_t my_thread_ids[WORKER_NUMBER];
	pthread_t my_group_thread_ids[GROUP_NUMBER];

	int lfd, cfd;
	int efd;   // 保存二叉树的树根
	struct epoll_event temps[OPEN_MAX];    //保存所有有请求的客户端

	//共享资源----临界资源
	std::list<int> waiting_for_service;  //保存等待服务的客户端
	std::map<int, std::string> all_fds;               //保存所有已经连接的客户端
	std::deque<std::string>  msg_que;    //消息队列

	/*std::map<std::string, std::map<string, string>> *group1;      //群组聊天消息队列
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

	bool Stop;  //总开关

	//锁与条件变量

	//接收连接的锁
	pthread_mutex_t my_accept_mutex = PTHREAD_MUTEX_INITIALIZER;

	//处理服务请求的锁，锁----waiting_for_service
	pthread_mutex_t my_client_mutex = PTHREAD_MUTEX_INITIALIZER;

	//处理连接请求，压入连接集合的锁， 锁------all_fds
	pthread_mutex_t my_cli_accept_mutex = PTHREAD_MUTEX_INITIALIZER;

	//发送信息锁，锁------msg_que
	pthread_mutex_t m_send_mutex = PTHREAD_MUTEX_INITIALIZER;

	//群聊消息map  list锁
	pthread_mutex_t m_send_group_mutex = PTHREAD_MUTEX_INITIALIZER;

	//群聊服务唤醒条件变量
	pthread_cond_t m_send_group_cond = PTHREAD_COND_INITIALIZER;

	//接收连接的条件变量
	pthread_cond_t my_accept_cond = PTHREAD_COND_INITIALIZER;

	//工作条件变量
	pthread_cond_t my_cli_cond = PTHREAD_COND_INITIALIZER;

	pthread_cond_t my_client_cond = PTHREAD_COND_INITIALIZER;


	//发送条件变量
	pthread_cond_t my_send_cond = PTHREAD_COND_INITIALIZER;

	//数据库变量
	MYSQL *mysql;
	MYSQL_RES *mysql_res;
	MYSQL_ROW mysql_row;
};



#endif
