#include "MyServer.h"

MyRector::MyRector(){
	lfd = 0;
	cfd = 0;
	efd = 0;
	Stop = false;
	mysql = NULL;
	mysql_res = NULL;
/*	group1 = NULL;      //Ⱥ��������Ϣ����
	group1_msg = NULL;

	group2 = NULL;
	group2_msg = NULL;

	group3 = NULL;
	group3_msg = NULL;

	group4 = NULL;
	group4_msg = NULL;

	group5 = NULL;
	group5_msg = NULL;
*/
	group_msg_list.push_back(NULL);
	group_msg_list.push_back(NULL);
	group_msg_list.push_back(NULL);
	group_msg_list.push_back(NULL);
	group_msg_list.push_back(NULL);


}
MyRector::~MyRector(){


}

struct Arg{
	MyRector* rec_this;
};

int MyRector::connect(){
	mysql = mysql_init(NULL);
	if (mysql == NULL){    //�������ʧ��
		std::cout << "mysql initialize failed" << std::endl;
		return -1;
	}
	
	mysql = mysql_real_connect(mysql, "localhost", "root", "root", "my_qq", 0, NULL, 0);    //ע������Ķ˿ں��ǲ���Ҫ�ı�
	if (!mysql){
		std::cout << "mysql connection falied" << std::endl;
	}
	else{
		std::cout << "mysql connecting success..." << std::endl;
	}
	return 0;
}

int MyRector::Mysql_query(const char *query){
	int res = mysql_query(mysql, query);
	std::cout<<"MYSQL -> res: "<<res<<std::endl;
	mysql_res = mysql_store_result(mysql);
	if(mysql_res != NULL){
		int rows = 0;
		rows =  mysql_num_rows(mysql_res);
	//	mysql_row = mysql_fetch_row(mysql_res);
	//	std::cout<<mysql_row[0]<<std::endl;
		std::cout<<"select numbers of rows: "<<rows<<std::endl;
		if (rows <= 0){  //no result
			return -1;
		}
	}
	if(res){  //operation failed
		std::cout << "MySql Query Failed" << std::endl;
		return -2;
	}
	return 0;
}


bool MyRector::Init(char *IP, int port){

	//���������ݿ�
	int ret = connect();
	if (ret == -1){
		std::cout << "Connect Mysql Failed" << std::endl;
	}

	if (!create_server_listener(IP, port)){   //��lfd�뱾��IP,�˿�
		std::cout << "Bind lfd Failed" << std::endl;
		return false;
	}
	Arg *arg = new Arg;
	arg->rec_this = this;
	std::cout << "group msg list: " << group_msg_list.size() << std::endl;          //�������󣬺���ɾ��
	pthread_create(&my_accept_id, NULL, accept_thread_proc, (void *)arg);
	pthread_create(&my_send_id, NULL, send_thread_proc, (void*)arg);
	for (int i = 0; i < WORKER_NUMBER; ++i){
		pthread_create(&my_thread_ids[i], NULL, woker_thread_proc, (void*)arg);
	}
	for (int j = 0; j < GROUP_NUMBER; ++j){
		pthread_create(&my_group_thread_ids[j], NULL, group_thread_proc, (void*)arg);
	}
	return true;
}

bool MyRector::Uninit(){
	Stop = true;
	shutdown(lfd, SHUT_RDWR);
	close(lfd);
	close(efd);

	//�ͷ�mysql���ڴ�
	mysql_close(mysql);
	mysql = NULL;

	return true;
}

bool MyRector::main_loop(MyRector *rec){
	MyRector* Rec = rec;
	std::cout << "Main thread id: " << pthread_self() << std::endl;
	while (!Rec->Stop){
		struct epoll_event temps[OPEN_MAX];
		int n = epoll_wait(Rec->efd, temps, OPEN_MAX, 10);             //Ϊʲô����Ҫ�ô������Ĳ����أ���������
		//����������epoll_wait��ʱ�������ú���
		if (n == 0){     //�������ȴ�����Ϊ������ƣ�ÿ10ms���ҿ�����ѭ����������ʱ�ر�����loop
			continue;
		}
		else if (n < 0){   //�ȴ�����Ҳ���˳�
			std::cout << "epoll_wait error" << std::endl;
			continue;
		}
		else{  //���������ʱ��
			int m = std::min(n, 1024);   //��ֹ������� 
			for (int i = 0; i < m; ++i){

				if (temps[i].data.fd == Rec->lfd){    //�������������
					pthread_cond_signal(&Rec->my_accept_cond);
				}
				else{    //����Ƿ�������
					pthread_mutex_lock(&Rec->my_client_mutex);
					Rec->waiting_for_service.push_back(temps[i].data.fd);
					pthread_mutex_unlock(&Rec->my_client_mutex);
					pthread_cond_signal(&Rec->my_client_cond);
				}
			}
		}
	}
	std::cout << "main_loop exit" << std::endl;
	return true;
}

bool MyRector::close_client(int cfd){
	int n = epoll_ctl(efd, EPOLL_CTL_DEL, cfd, NULL);
	if (n == -1){
		std::cout << "epoll_ctl delete cfd failed" << std::endl;
		return false;
	}
	//std::map<int, std::string>::iterator begin = all_fds.begin();
	//std::map<int, std::string>::iterator end = all_fds.end();
	
	close(cfd);
	return true;
}

bool MyRector::create_server_listener(char* IP, int port){

	if ((lfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1){
		std::cout << "Get lfd  failed" << std::endl;
		return false;
	}
	struct sockaddr_in server_addr;

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	inet_pton(AF_INET, IP, &server_addr.sin_addr.s_addr);

	if ((bind(lfd, (struct sockaddr*)&server_addr, sizeof(server_addr))) == -1){
		std::cout << "bind error" << std::endl;
		return false;
	}

	if (listen(lfd, 50) == -1){
		std::cout << "listen error " << std::endl;
		return false;
	}

	efd = epoll_create(20);   //�½�һ��������
	if (efd == -1){
		std::cout << "epoll_create error" << std::endl;
		return false;
	}

	struct epoll_event temp;
	memset(&temp, 0, sizeof(temp));
	temp.data.fd = lfd;
	temp.events = EPOLLIN | EPOLLRDHUP;

	if ((epoll_ctl(efd, EPOLL_CTL_ADD, lfd, &temp)) == -1){
		std::cout << "epoll_ctl lfd error" << std::endl;
		return false;
	}

	return true;
}


void* MyRector::accept_thread_proc(void* rec){    //����������߳�
	Arg *A = static_cast<Arg*>(rec);
	MyRector *Rec = A->rec_this;

	while (!Rec->Stop){
		pthread_mutex_lock(&Rec->my_accept_mutex);
		pthread_cond_wait(&Rec->my_accept_cond, &Rec->my_accept_mutex);  //�����ȴ����������������������

		struct sockaddr_in client_addr;
		socklen_t client_len;
		client_len = sizeof(client_addr);
		int new_cfd = accept(Rec->lfd, (struct sockaddr*)&client_addr, &client_len);
		pthread_mutex_unlock(&Rec->my_accept_mutex);

		if (new_cfd == -1){
			continue;
		}

		//������µ�cfd���뵽�ͻ�������
		pthread_mutex_lock(&Rec->my_cli_accept_mutex);
		Rec->all_fds.insert(std::make_pair(new_cfd, ""));
		pthread_mutex_unlock(&Rec->my_cli_accept_mutex);

		//�޸Ĳ�������
		int flag = fcntl(new_cfd, F_GETFL, 0);
		flag |= O_NONBLOCK;
		int res = fcntl(new_cfd, F_SETFL, flag);
		if (res == -1){
			std::cout << "fcntl new_fd failed" << std::endl;
			continue;
		}

		//������½�����cfd���뵽����������
		struct epoll_event temp;
		temp.data.fd = new_cfd;
		temp.events = EPOLLIN | EPOLLET;  //���ش��������¼�
		if ((epoll_ctl(Rec->efd, EPOLL_CTL_ADD, new_cfd, &temp)) == -1){
			std::cout << "epoll_ctl  new_cfd failed" << std::endl;
		}
		//int pthread_cond_destroy(Rec->my_accept_cond);
	}
	return NULL;
}



void* MyRector::woker_thread_proc(void *rec){
	Arg *A = static_cast<Arg*>(rec);
	MyRector *Rec = A->rec_this;
	while (!Rec->Stop){
		int cfd;

		pthread_mutex_lock(&Rec->my_client_mutex);
		if (Rec->waiting_for_service.empty()){
			pthread_cond_wait(&Rec->my_client_cond, &Rec->my_client_mutex);
		}
		cfd = Rec->waiting_for_service.front();
		Rec->waiting_for_service.pop_front();
		pthread_mutex_unlock(&Rec->my_client_mutex);

		std::cout << std::endl;

		time_t now = time(NULL);
		struct tm* now_str = localtime(&now);
		std::ostringstream OS;

		std::string client_str = "";
		char buf[256];
		bool Error = false;


		//������message���
		bool last_part = false;
		int target_cfd = -1;
		std::string user_from_msg = "";

		//Ⱥ�������õ���
		bool last_part_group = false;
		std::string user_msg_to_group = "";
		std::string group_name_for_chat = "";
		std::string user_for_group_chat = "";

		while (1){    //�Ѿ������˲������������ش���
			memset(buf, 0, sizeof(buf));
			int read_n = read(cfd, buf, sizeof(buf));
			if (read_n == -1){
				if (errno == EWOULDBLOCK)   //���ﲻ����Error
					break;
				else{
					Rec->close_client(cfd);
					Error = true;
					break;
				}
			}
			else if (read_n == 0){   //�Զ˹ر�

				std::string user_name = Rec->all_fds[cfd];
				std::cout << user_name << " PREPARING OFFLINE..." << std::endl;
				std::string query_str = "update User_Info set state = \'NOT_ONLINE\' where user_name = \'" + user_name + "\';";
				const char* query = query_str.c_str();
				int ret = Rec->Mysql_query(query);
				if (ret == -2){
					std::cout << "OFFLINE FAILED" << std::endl;
				}
				else{
					std::cout << user_name << "OFFLINE..." << std::endl;
				}
				//�ӿͻ��˵��б���ɾ��
				pthread_mutex_lock(&Rec->my_cli_accept_mutex);
				Rec->all_fds.erase(cfd);
				pthread_mutex_unlock(&Rec->my_cli_accept_mutex);

				std::cout << "client abort connection" << std::endl;
				Rec->close_client(cfd);
				Error = true;
				break;
			}

			if (buf[0] == REGISTER){  //�����������ע���˺�
				std::string get_str(buf);
				std::size_t find_t = get_str.find('\t');
				std::string user_name_str = get_str.substr(1, find_t-1);
				std::string password_str = get_str.substr(find_t + 1, get_str.size() - 2 - find_t);    //���������з�
				//const char* user_name = user_name_str.c_str();
				//const char* password = pass_word_str.c_str();

				std::cout << "user_name: " << user_name_str << "     password: " << password_str << std::endl;
				std::cout << std::endl;

				std::string query_str = "select password from User_Info where user_name = \'" + user_name_str + "\';";
				const char* query = query_str.c_str();
				int ret = Rec->Mysql_query(query);			//mysql��ѯ���
				Rec->mysql_res = mysql_store_result(Rec->mysql);   //�Ѳ�ѯ�Ľṹ�洢��mysql_res
				if (ret != -1){  //����鵽���и��û�������ô�����ø��û���ע��
					client_str += "this user_name has been registered!";
				}
				else{   //���û�в�ѯ�����û�������ô���Խ���ע��
					query_str.clear();
					query_str = "insert into User_Info(user_name,password,state) values(\'" + user_name_str + "\', \'" + password_str + "\', \'ONLINE\');";
					const char *query = query_str.c_str();
					ret = Rec->Mysql_query(query);
					if (ret != -2){    //ע��ɹ��� ���½�һ����洢���û��ĺ����б�Ŀǰֻ�����Լ�
						std::cout<<"Insert into user_Info Success..."<<std::endl;
						client_str += "insert into User_Info  success...";
						query_str.clear();
						query_str = "create table " + user_name_str + "(friend_name VARCHAR(20));";
						query = query_str.c_str();
						if ((ret = Rec->Mysql_query(query)) != -2){
							std::cout << "create friend_table success..." << std::endl;
						}
						query_str.clear();
						query_str = "insert into " + user_name_str + " values(\'" + user_name_str + "\');";
						query = query_str.c_str();
						ret = Rec->Mysql_query(query);
						std::cout<<"Friend table insert selft -> ret: "<<ret<<std::endl;
						if (ret != -2){
							std::cout << "insert self to friend_table success..." << std::endl;
						}

						client_str += "register success...";
						/*if (mysql_errno(Rec->mysql)){
							std::cout << "mysql Error..." << std::endl;
						}*/
						continue;
					}
					else{
						client_str += "insert failed ";
					}
				}
			}
			else if (buf[0] == LOGIN){         //����������ǵ�¼��Ϣ
				std::string get_str(buf);
				std::size_t find_t = get_str.find('\t'); 
				std::string user_name_str = get_str.substr(1, find_t - 1);   //�û���
				std::string password_str = get_str.substr(find_t + 1, get_str.size() - 2-find_t);   //����  �������з�

				std::cout << "user_name: " << user_name_str << "     password: " << password_str << std::endl;
				std::cout << std::endl;
				std::string query_str;
				query_str = "select password from User_Info where user_name = \'" + user_name_str + "\';";
				const char *query = query_str.c_str();
				std::cout<<"Login order: "<<query<<std::endl;
				int ret = Rec->Mysql_query(query);
				if (ret != -1){
					//Rec->mysql_res = mysql_store_result(Rec->mysql);
					//if (Rec->mysql_res){   //���ȡ��������
						Rec->mysql_row = mysql_fetch_row(Rec->mysql_res);
						if (Rec->mysql_row){  //��ȡ����һ������
							const char* password_char = password_str.c_str();
							std::cout<<"MYSQL password: "<<Rec->mysql_row[0]<<std::endl;
							if (strcmp(Rec->mysql_row[0], password_char) == 0){
								client_str += " Login in success...";

								//���׽��ֺ��û���
								pthread_mutex_lock(&Rec->my_cli_accept_mutex);
								Rec->all_fds[cfd] = user_name_str;
								pthread_mutex_unlock(&Rec->my_cli_accept_mutex);

								//���ݿ���û�״̬���ó�����

								query_str.clear();
								query_str = "update User_Info set state = \'ONLINE\' where user_name = \'" + user_name_str + "\';";
								query = query_str.c_str();
								int ret_3 = Rec->Mysql_query(query);
								if (ret_3 == -2){
									client_str += "Change State Failed...";
								}
								else{
									client_str += "Change State To ONLINE...";
								}
								continue;
							}
							else{
								client_str += "password is wrong";
							}
							/*if (mysql_errno(Rec->mysql)){
								std::cout <<"mysql_error(pReactor->mysql)" << std::endl;
							}*/
						}
						else{
							client_str += "You have to register first";
						}
					//}
				}
				else{
					std::cout << "login query error" << std::endl;
				}	
			}
			//������Ϣ����
			else if (buf[0] == MESSAGE){
				client_str += buf;
			}
			else if(buf[0] == ADD_FRIEND){
				std::string get_str(buf);
				std::size_t find_t = get_str.find('\t');
				std::string user_name_to = get_str.substr(1, find_t - 1);   //��������
				std::string user_name_from = get_str.substr(find_t + 1, get_str.size() - 2-find_t);   //������

				std::cout << "friend request: " << user_name_to << "     from user: " << user_name_from << std::endl;
				std::cout << std::endl;

				//���Ȳ�ѯһ������ӵ�ID�ڲ������ݿ���
				std::string query_str;
				query_str = "select password from User_Info where user_name = \'" + user_name_to + "\';";
				const char *query = query_str.c_str();
				int ret = Rec->Mysql_query(query);
				if (ret != -1 || ret == -2){

					//��ѯһ���ǲ����Ѿ����Լ��ĺ����б���
					query_str.clear();
					query_str = "select friend_name from "+user_name_to + " where friend_name = \'" + user_name_from + "\';";  //�鿴�Է��Ƿ�����
					query = query_str.c_str();
					int ret_find = Rec->Mysql_query(query);
					if (ret_find == -1){  //����Է������Լ��ĺ����б��У������������Ӳ���
						query_str.clear();
						query_str = "select state from User_Info where user_name = \'" + user_name_to + "\';";  //�鿴�Է��Ƿ�����
						query = query_str.c_str();
						int ret2 = Rec->Mysql_query(query);
						if (ret2 != -1){
							Rec->mysql_row = mysql_fetch_row(Rec->mysql_res);         //�õ��Է��û���Ŀǰ״̬
							if (Rec->mysql_row){  //��ȡ����һ������
								const char* online = "ONLINE";
								std::cout << "MYSQL password: " << Rec->mysql_row[0] << std::endl;
								if (strcmp(Rec->mysql_row[0], online) == 0){
									client_str += "Request ID Is ONLINE";

									//����Map��user_name �� cfd��Ӧ����
									int request_id = -1;
									std::map<int, std::string>::iterator begin = (Rec->all_fds).begin();
									std::map<int, std::string>::iterator end = (Rec->all_fds).end();
									while (begin != end){
										if ((*(begin)).second == user_name_to){
											request_id = (*begin).first;
										}
										begin++;
									}
									if (request_id == -1){
										client_str += "Find Request ID Failed";
									}
									else{
										std::string send_to_user_to = user_name_from + "\t ASKING FOR FRIENDSHIP, YES OR NO?\n";
										int w = write(request_id, send_to_user_to.c_str(), send_to_user_to.size());
										if (w == -1){
											client_str += "SEND REQUEST FAILED";
										}
										else{
											client_str += "SEND REQUEST ALREADY";
										}
									}
								}
								else{
									client_str += "Request ID IS NOT ONLINE";
								}
							}
						}
						else{
							client_str = "Get State Failed";
						}
					}
					else{   //����Է��Ѿ����Լ��ĺ����б��У��򷵻ش�����Ϣ
						client_str += "The ID You Request Already In Your Friend List...";
					}
				}//-----------
				else{    //���Ҫ�ӵĺ��Ѳ����ڣ���ô�ͷ��ش�����Ϣ
					client_str = "The ID You Request Not Exists...";
				}
			}
			else if (buf[0] == ANSWER_FRIEND){
				std::string get_str(buf);
				std::size_t find_t = get_str.find('\t');
				std::string answer = get_str.substr(1, find_t - 1);   //�û���
				std::string last = get_str.substr(find_t + 1, get_str.size() - 2 - find_t);
				std::size_t find_t2 = last.find('\t');
				std::string user_name_answer = last.substr(0, find_t2);   //
				std::string user_name_request = last.substr(find_t2 + 1, last.size() - find_t2 - 1);   

				std::cout << user_name_answer << " answer: " << answer << " to " << user_name_request << std::endl;
				std::cout << std::endl;


				//�Ȳ�ѯһ�¶Է����ڲ�
				int request_id = -1;
				std::map<int, std::string>::iterator begin = Rec->all_fds.begin();
				std::map<int, std::string>::iterator end = Rec->all_fds.end();
				while (begin != end){
					if ((*begin).second == user_name_request){
						request_id = (*begin).first;
						break;
					}
					begin++;
				}
				//����Է��Ѿ����ߣ������
				if (request_id == -1){
					client_str += "The Other Side Already OFFLINE, Answer Not Send...";
					continue;
				}
				else{  //����Է�������

					//���ͬ��, ��˫���������Լ��ĺ��ѱ�
					if (answer == "YES" || answer == "y" || answer == "Y" || answer == "ͬ��"){
						std::string query_str = "";
						query_str = "insert into " + user_name_request + " values(\'" + user_name_answer + "\');";
						const char* query = query_str.c_str();
						int ret_4 = Rec->Mysql_query(query);
						if (ret_4 != -2){
							//�ȶԷ����Լ�����ˣ��Լ�����ӶԷ�
							client_str += "The Other Side Already Add Self to His/Her Friend List...";
							query_str.clear();
							query_str = "insert into " + user_name_answer + " values(\'" + user_name_request + "\');";
							query = query_str.c_str();
							int ret_4_1 = Rec->Mysql_query(query);
							if (ret_4_1 != -2){
								write(request_id, "The Other Side Already Add Self to His/Her Friend List...\n", sizeof("The Other Side Already Add Self to His/Her Friend List...\n"));
								client_str += "Already add " + user_name_request + " to your frined list";
							}
							else{
								client_str += "Failed to Add " + user_name_request + " to Friend List...";
							}
						}
						else{
							client_str += user_name_request + "Failed to Add Self to His/Her Friend List...";
						}
					}
					else{   //�����ͬ��
						write(request_id, "The Other Side Refused Your Friend Request...", sizeof("The Other Side Refused Your Friend Request..."));
					}

				}
			}
			else if (buf[0] == SINGLE_CHAT || last_part){
				/*std::string get_str(buf);
				std::size_t find_t = get_str.find('\t');
				std::string user_name_to = get_str.substr(1, find_t - 1);   //��������
				std::string user_name_from = get_str.substr(find_t + 1, get_str.size() - 2 - find_t);   //������
				//�������ȡ���͵���Ϣ
				std::string message = "";*/

				if (!last_part){  //����ǵ�һ�ν������õ�
					std::string get_str(buf);
					std::size_t find_t = get_str.find('\t');
					std::string user_name_to = get_str.substr(1, find_t - 1);   //�û���

					std::string last = get_str.substr(find_t + 1, get_str.size() - 1 - find_t);  //�������һ���ַ�����Ȼ�п�����'\n'
					std::size_t find_t2 = last.find('\t');
					std::string user_name_from = last.substr(0, find_t2);   //Դ�û�
					std::string message = last.substr(find_t2 + 1, last.size() - find_t2 - 1);   //��Ϣ

					//std::cout << user_name_to << "_to_" << user_name_from << "_message: " << message << "__" << std::endl;
					//std::cout << std::endl;

					std::cout << "message to: " << user_name_to << "     from user: " << user_name_from << std::endl;
					std::string query_str = "select user_name from User_Info where user_name = \'" + user_name_from + "\';";
					const char* query = query_str.c_str();
					int ret = Rec->Mysql_query(query);
					if (ret != -1 && ret != -2){
						query_str.clear();
						query_str = "select friend_name from " + user_name_from + " where friend_name = \'" + user_name_to + "\';";
						query = query_str.c_str();
						int ret_1 = Rec->Mysql_query(query);
						if (ret_1 != -1 && ret_1 != -2){
							std::map<int, std::string>::iterator begin = Rec->all_fds.begin();
							std::map<int, std::string>::iterator end = Rec->all_fds.end();
							int request_id = -1;
							while (begin != end){
								if ((*begin).second == user_name_to){
									request_id = (*begin).first;
									break;
								}
								begin++;
							}
							if (request_id == -1){
								client_str += "The ID Is OFFLINE...";
							}
							else{

								//�б�Ҫȥ�ٿ��̣߳�����������������������������������������������������������,����߳�����̫���ˣ���˵
								target_cfd = request_id;
								client_str += message;
								last_part = true;    //���ڶ�ʣ�µ���Ϣ
								user_from_msg = user_name_from;  //�����ڵ���ģʽ�У�ע����Ϣ��Դ
								continue;
							}
						}
						else{
							client_str += "The ID You Request Is Not In Your Frined list...";
						}
					}
					else{
						client_str += "You Are Not Registered User, Please Registered first...";
					}
				}
				else{
					if (target_cfd != -1){
						std::string message_last(buf);
						client_str += message_last;
						continue;
					}
					else{
						client_str += "THE ID OFFLINE...";
					}
				}
			}
			else if(buf[0] == SEARCH_ID) {
				std::string get_str(buf);
				std::size_t find_t = get_str.find('\t');
				std::string user_name_str = get_str.substr(1, find_t-1);   //�û���

				std::cout << "serching ID: " << user_name_str << std::endl;
				std::cout << std::endl;
				std::string query_str;
				query_str = "select password from User_Info where user_name = \'" + user_name_str + "\';";
				const char *query = query_str.c_str();
				std::cout << "Login order: " << query << std::endl;
				int ret = Rec->Mysql_query(query);
				if (ret == -1){
					//write(cfd, "ID NOT EXISTS", sizeof("ID NOT EXISTS"));
					client_str += "ID NOT EXISTS";
				}
				else{
					//write(cfd, "ID EXISTS", sizeof("ID EXISTS"));
					client_str += "ID EXISTS";
				}
			}
			else if (buf[0] == GROUP_CHAT || last_part_group){   //��Ⱥ�﷢����Ϣ
				if (!last_part_group){  //����ǵ�һ�ν������õ�
					std::string get_str(buf);
					std::size_t find_t = get_str.find('\t');
					std::string group_name = get_str.substr(1, find_t - 1);   //�û���

					std::string last = get_str.substr(find_t + 1, get_str.size() - 1 - find_t);  //�������һ���ַ�����Ȼ�п�����'\n'
					std::size_t find_t2 = last.find('\t');
					std::string user_name_from = last.substr(0, find_t2);   //Դ�û�
					std::string message = last.substr(find_t2 + 1, last.size() - find_t2 - 1);   //��Ϣ

					std::cout << "message to: " << group_name << "     from user: " << user_name_from << std::endl;
					std::cout << std::endl;
					std::string query_str = "select * from " + group_name + " limit 1;";;
					const char* query = query_str.c_str();
					int ret = Rec->Mysql_query(query);
					if (ret != -1 && ret != -2){  //���Ⱥ�����
						client_str +=  message;      
						last_part_group = true;
						group_name_for_chat = group_name;
						user_for_group_chat = user_name_from;
						std::cout<<"GROUP ID EXISTS fucking this shit..."<<std::endl;
						continue;
					}
					else{
						client_str += "GROUP ID NOT EXISTS...\n";
					}
				}
				else{
					std::cout<<"KEPPING GETTING MESSAGE..."<<std::endl;
					std::string buffer(buf);
					client_str += buf;
					continue;
				}
			}
			else if (buf[0] == BUILD_GROUP){  //�½�һ��Ⱥ��,���Լ�����ΪȺ��
				std::string get_str(buf);
				std::size_t find_t = get_str.find('\t');
				std::string group_owner = get_str.substr(1, find_t - 1);   
				std::string group_name = get_str.substr(find_t + 1, get_str.size() - 2 - find_t);   
				std::cout << group_owner << " BUILD GROUP: " << group_name << std::endl;

				std::string query_str = "select password from User_Info where user_name = \'" + group_owner + "\';";
				const char* query = query_str.c_str();
				int ret = Rec->Mysql_query(query);
				if (ret != -1){
					query_str.clear();
					query_str = "create table " + group_name + "(user_name varchar(20) unique, owner varchar(20) default \'NO\');";
					query = query_str.c_str();
					int ret_1 = Rec->Mysql_query(query);
					if (ret_1 != -1){
						query_str.clear();
						query_str = "insert into " + group_name + "(user_name, owner) values(\'" + group_owner + "\', \'YES\');";
						query = query_str.c_str();
						int ret_2 = Rec->Mysql_query(query);
						if (ret_2 != -2 && ret_2 != -1){
							client_str += "CREATE GROUP SUCCESS...\nADD SELF TO GROUP...\n";
						}
						else{
							client_str += "Failed Add Self to Group";
						}
					}
					else{
						client_str += "CREATE GROUP FAILED...";
					}
				}
				else{
					client_str += "Please Register First...";
				}
			}
			else if (buf[0] == ANSWER_ADD_GROUP){ //��Ӧ�Ƿ����Ⱥ��
				std::string get_str(buf);
				std::size_t find_t = get_str.find('\t');
				std::string answer = get_str.substr(1, find_t - 1);   //��Ӧ
				std::string last = get_str.substr(find_t + 1, get_str.size() - 2 - find_t);
				std::size_t find_t2 = last.find('\t');
				std::string group_name = last.substr(0, find_t2);      //���������Ⱥ��
				std::string user_name_answer = last.substr(find_t2 + 1, last.size() - find_t2 - 1); //��Ӧ���û�

				std::cout << user_name_answer << " answer: " << answer << " to GROUP��" << group_name << std::endl;
				std::cout << std::endl;

				if (answer == "YES" || answer == "y" || answer == "Y" || answer == "ͬ��"){
					std::string query_str = "insert into " + group_name + "(user_name) values(\'" + user_name_answer + "\');";
					const char* query = query_str.c_str();
					int ret = Rec->Mysql_query(query);
					if (ret != -2){
						client_str += "Add in GROUP: " + group_name + " Successly...\n";
					}
					else{
						client_str += "Failed Add in GROUP: " + group_name + "...\n";
					}
				}
				else{
					client_str += "You Refused the Group Invision...\n";
				}

			}
			else if (buf[0] == ADD_USER_TO_GROUP){ //��һ���û����Լ�����, ���ͻ�����Ҫ��������Լ����û�����,�������û��Լ���ӣ��ͻ��˳����Զ����
				std::string get_str(buf);
				std::size_t find_t = get_str.find('\t');
				std::string group_name = get_str.substr(1, find_t - 1);   //����

				std::string last = get_str.substr(find_t + 1, get_str.size() - 2 - find_t);  //ȥ�����һ��'\n'
				std::size_t find_t2 = last.find('\t');
				std::string request_user = last.substr(0, find_t2);   //����������
				std::string target_user = last.substr(find_t2 + 1, last.size() - find_t2 - 1);   //����������

				std::string query_str = "select owner from " + group_name + " where user_name = \'" + request_user + "\';";
				const char* query = query_str.c_str();
				int ret = Rec->Mysql_query(query);
				if (ret != -1 && ret != -2){
					Rec->mysql_row = mysql_fetch_row(Rec->mysql_res);
					if (Rec->mysql_row){  //��ȡ����һ������
						const char* request_user_char = "YES";
						if (strcmp(Rec->mysql_row[0], request_user_char) == 0){
							query_str.clear();
							query_str = "select password from User_Info where user_name = \'" + target_user + "\';";  //�����Է��Ƿ����......
							query = query_str.c_str();
							int ret_2 = Rec->Mysql_query(query);
							if (ret_2 != -1){
								std::map<int, std::string>::iterator begin = Rec->all_fds.begin();
								std::map<int, std::string>::iterator end = Rec->all_fds.end();
								int request_id = -1;
								while (begin != end){
									if ((*begin).second == target_user){
										request_id = (*begin).first;
										break;
									}
									begin++;
								}
								if (request_id == -1){     //�Է�������
									client_str += "THE OTHER SIDE IS OFFLINE...\n";
								}
								else{   //�Է�����
									std::string request_msg = group_name + "	group invide you to add in...YES or NO?\n";  
									int w = write(request_id, request_msg.c_str(), request_msg.size());
									if (w == -1){
										client_str += "FAILED TO SEND INVIDING MESSAGE...\n";
									}
								}
							}
							else{
								client_str += "ID NOT EXISTS...\n";
							}
						}
						else{
							client_str += "YOU ARE NOT AUTHORRIZED TO ADD NEW GROUP MEMBER...";
						}
					}
					else{
						client_str += "Failed to Get Group Owner...\n";
					}
				}
				else{
					client_str += "GROUP ID NOT EXISTS   OR   YOU ARE NOT A MEMBER OF THIS GROUP...\n";
				}
			}
			else if (buf[0] == SEARCH_GROUP){   //��ѯһ�����Ƿ����
				std::string get_str(buf);
				std::size_t find_t = get_str.find('\t'); 
				std::string group_name = get_str.substr(1, find_t - 1);  //�õ�Ⱥ��
				std::string query_str = "select * from " + group_name + " limit 1;";   //ֻȡ��һ�У�ֻΪ����֤�Ƿ��������
				const char* query = query_str.c_str();
				int ret = Rec->Mysql_query(query);
				if (ret != -1){
					client_str += "GROUP ID EXISTS fuck try...\n";
				}
				else{
					client_str += "GROUP ID NOT EXISTS...\n";
				}
			}
			else if (buf[0] == REQUEST_ADD_GROUP){
				std::string get_str(buf);
				std::size_t find_t = get_str.find('\t');
				std::string group_name = get_str.substr(1, find_t - 1);   //����Ҫ�ӵ���
				std::string user_request = get_str.substr(find_t + 1, get_str.size() - 2 - find_t);   //������
				std::string query_str = "select * from " + group_name + " limit 1;";   //ֻȡ��һ�У�ֻΪ����֤�Ƿ�������� 
				const char* query = query_str.c_str();
				int ret = Rec->Mysql_query(query);
				if (ret != -1 && ret != -2){   //�Ȳ�ѯ��ID�Ƿ����
					//�鿴Ⱥ���Ƿ�����
					Rec->mysql_row = mysql_fetch_row(Rec->mysql_res);
					if (Rec->mysql_row){  //��ȡ����һ������
						std::string group_owner(Rec->mysql_row[0]);
						std::map<int, std::string>::iterator begin = Rec->all_fds.begin();
						std::map<int, std::string>::iterator end = Rec->all_fds.end();
						int request_id = -1;
						while (begin != end){
							if ((*begin).second == group_owner){
								request_id = (*begin).first;
								break;
							}
							begin++;
						}
						if (request_id == -1){     //�Է�������
							client_str += "GROUP MANAGER IS OFFLINE...\n";
						}
						else{   //�Է�����
							std::string request_msg = user_request + "	asking for joining in your group...YES or NO?\n"; //������Ҫ������ȡ�����ߵ��˻�����
							int w = write(request_id, request_msg.c_str(), request_msg.size());
							if (w == -1){
								client_str += "FAILED TO SEND INVIDING MESSAGE...\n";
							}
						}
					}
					else{
						client_str += "FAILED TO CHECK GROUP MANAGER...\n";
					}
				}
				else{
					client_str += "GROUP ID NOT EXISTS...\n";
				}
			}
			else if (buf[0] == ANSWER_REQUEST_GROUP){
				std::string get_str(buf);
				std::size_t find_t = get_str.find('\t');
				std::string answer = get_str.substr(1, find_t - 1);   //��Ӧ
				std::string last = get_str.substr(find_t + 1, get_str.size() - 2 - find_t);
				std::size_t find_t2 = last.find('\t');
				std::string group_name = last.substr(0, find_t2);      //Ⱥ��
				std::string request_user = last.substr(find_t2 + 1, last.size() - find_t2 - 1); //������û�

				std::map<int, std::string>::iterator begin = Rec->all_fds.begin();
				std::map<int, std::string>::iterator end = Rec->all_fds.end();
				int request_id = -1;
				while (begin != end){
					if ((*begin).second == request_user){
						request_id = (*begin).first;
						break;
					}
					begin++;
				}
				if (answer == "YES" || answer == "y" || answer == "Y" || answer == "ͬ��"){
					std::string query_str = "insert into " + group_name + "(user_name) values(\'" + request_user + "\');";   //ֻȡ��һ�У�ֻΪ����֤�Ƿ�������� 
					const char* query = query_str.c_str();
					int ret = Rec->Mysql_query(query);
					if (ret != -1 && ret != -2){   //�Ȳ�ѯ��ID�Ƿ����
						if (request_id == -1){     //�Է�������
							client_str += "THE REQUESTER IS OFFLINE...\n";
						}
						else{   //�Է�����
							std::string request_msg = group_name + "	agreed with your request...\n"; //������Ҫ������ȡ�����ߵ��˻�����
							int w = write(request_id, request_msg.c_str(), request_msg.size());
							if (w == -1){
								client_str += "FAILED TO SEND INVIDING MESSAGE...\n";
							}
						}
					}
					else{
						client_str += "FAILED TO ADD REQUEST USER INTO GROUP...\n";
					}
				}
				else{  //����ܾ������Է����;ܾ���Ϣ

					if (request_id == -1){     //�Է�������
						client_str += "THE REQUESTER IS OFFLINE...\n";
					}
					else{   //�Է�����
						std::string request_msg = group_name + "	refused your request...\n"; //������Ҫ������ȡ�����ߵ��˻�����
						int w = write(request_id, request_msg.c_str(), request_msg.size());
						if (w == -1){
							client_str += "FAILED TO SEND  MESSAGE...\n";
						}
					}
				}
			}

			else{
				std::cout << "unlegel request" << std::endl;
				break;
			}
		}//--------------------------------������Ϣ��while

		if (Error){
			continue;
		}

		if (last_part){
			last_part = false;
			OS << "[" << now_str->tm_year + 1900 << "-"
				<< std::setw(2) << std::setfill('0') << now_str->tm_mon + 1 << "-"
				<< std::setw(2) << std::setfill('0') << now_str->tm_mday << " "
				<< std::setw(2) << std::setfill('0') << now_str->tm_hour << ":"
				<< std::setw(2) << std::setfill('0') << now_str->tm_min << ":"
				<< std::setw(2) << std::setfill('0') << now_str->tm_sec << " ] ";
			client_str.insert(0, OS.str());
			user_from_msg += ": ";
			client_str.insert(0, user_from_msg);
			int w = write(target_cfd, client_str.c_str(), client_str.size());
			if (w != -1){
				write(cfd, "SEND MESSAGE SUCCESS...\n", sizeof("SEND MESSAGE SUCCESS..."));
			}
			else{
				write(cfd, "SEND MESSAGE FAILED...\n", sizeof("SEND MESSAGE FAILED..."));
			}
			target_cfd = -1;
			continue;
		}

		if (last_part_group){
			last_part_group = false;
			//�ȼ���Ƿ�����Լ�Ⱥ���������Ϣ����

			std::list<std::map<std::string, std::map<std::string, std::string>*>* >::iterator begin = Rec->group_msg_list.begin();
			std::list<std::map<std::string, std::map<std::string, std::string>*>* >::iterator end = Rec->group_msg_list.end();
			while (begin != end){
				if ((*begin) == NULL){
					Rec->group_msg_list.erase(begin);
					break;
				}
				begin++;
			}
			if (begin == end){
				//write(cfd, "SORRY, SERVICE BUSY...PLEASE TRY LATER...\n", sizeof("SORRY, SERVICE BUSY...PLEASE TRY LATER...\n"));
				client_str += "SORRY, SERVICE BUSY...PLEASE TRY LATER...\n";
			}
			else{//����
				//client_str��ʱ���
   				std::cout<<"jianlaile ma   shabi "<<std::endl;
				OS << "[" << now_str->tm_year + 1900 << "-"
					<< std::setw(2) << std::setfill('0') << now_str->tm_mon + 1 << "-"
					<< std::setw(2) << std::setfill('0') << now_str->tm_mday << " "
					<< std::setw(2) << std::setfill('0') << now_str->tm_hour << ":"
					<< std::setw(2) << std::setfill('0') << now_str->tm_min << ":"
					<< std::setw(2) << std::setfill('0') << now_str->tm_sec << " ] ";
				client_str.insert(0, OS.str());

				std::string resource = user_for_group_chat + "\n";  //��Ϣ��Դ
				client_str = resource + "  "+ client_str;
				//std::cout<<client_str<<std::endl;
				pthread_mutex_lock(&Rec->m_send_group_mutex);
				std::map<std::string, std::string> *this_group_msg = new std::map <std::string, std::string > ;
				this_group_msg->insert(make_pair(user_for_group_chat, client_str));
				std::map<std::string, std::map<std::string, std::string>*> *this_group = new std::map < std::string, std::map<std::string, std::string>* > ;
				this_group->insert(make_pair(group_name_for_chat, this_group_msg));   //��������һ����Ϣ�������һ������Ϣmap
				Rec->group_msg_list.push_back(this_group);
				//Rec->msg_que.push_back(client_str);      // �ǲ���ÿ��Ⱥ�鶼Ӧ�����Լ�����Ϣ���У��ǲ�����Ҫ��̬ȥ������Ϣ����
				pthread_mutex_unlock(&Rec->m_send_group_mutex);

				pthread_cond_signal(&Rec->m_send_group_cond);   //����Ⱥ���߳�
			}
			continue;
		}
		
		std::cout << "client message: " << client_str << std::endl;
		if (client_str[0] == MESSAGE){
			client_str.erase(0, 1);
			OS << "[" << now_str->tm_year + 1900 << "-"
				<< std::setw(2) << std::setfill('0') << now_str->tm_mon + 1 << "-"
				<< std::setw(2) << std::setfill('0') << now_str->tm_mday << " "
				<< std::setw(2) << std::setfill('0') << now_str->tm_hour << ":"
				<< std::setw(2) << std::setfill('0') << now_str->tm_min << ":"
				<< std::setw(2) << std::setfill('0') << now_str->tm_sec << " ] ";
			client_str.insert(0, OS.str());
		}
		else{
			client_str.push_back('\n');
			write(cfd, client_str.c_str(), client_str.size());
			continue;
		}

		pthread_mutex_lock(&Rec->m_send_mutex);
		Rec->msg_que.push_back(client_str);
		pthread_mutex_unlock(&Rec->m_send_mutex);
		pthread_cond_signal(&Rec->my_send_cond);
	}
	return NULL;
}


void* MyRector::send_thread_proc(void *rec){     //ϵͳȺ��������Ϣ
	Arg *A = static_cast<Arg*>(rec);
	MyRector *Rec = A->rec_this;

	while (!Rec->Stop){
		std::string send_msg;
		pthread_mutex_lock(&Rec->m_send_mutex);
		while (Rec->msg_que.empty())
			pthread_cond_wait(&Rec->my_send_cond, &Rec->m_send_mutex);
		send_msg = Rec->msg_que.front();
		Rec->msg_que.pop_front();
		pthread_mutex_unlock(&Rec->m_send_mutex);
		std::cout << std::endl;
		while (1){
			int send_n;
			int cfd;
			std::map<int,std::string>::iterator begin = Rec->all_fds.begin();
			std::map<int,std::string>::iterator end = Rec->all_fds.end();
			while (begin != end){
				cfd = (*(begin++)).first;
				send_n = write(cfd, send_msg.c_str(), send_msg.length());
				if (send_n == -1){
					if (errno == EWOULDBLOCK){
						sleep(1);
						continue;
					}
					else{
						std::cout << "send data, fd = " << cfd << std::endl;
						Rec->close_client(cfd);
						break;
					}
				}
			}
			send_msg.clear();
			if (send_msg.empty())
				break;
		}
	}
	return NULL;
}


void* MyRector::group_thread_proc(void *rec){
	Arg *A = static_cast<Arg*>(rec);
	MyRector *Rec = A->rec_this;
	while (!Rec->Stop){
		pthread_mutex_lock(&Rec->m_send_group_mutex);
		//while (Rec->msg_que.empty())
		pthread_cond_wait(&Rec->m_send_group_cond, &Rec->m_send_group_mutex);

		std::map<std::string, std::map<std::string, std::string>*> *buffer = NULL;
		std::list<std::map<std::string, std::map<std::string, std::string>*>* >::iterator  r_begin = Rec->group_msg_list.begin();
		std::list<std::map<std::string, std::map<std::string, std::string>*>* >::iterator  r_end = Rec->group_msg_list.end();
		while (r_begin != r_end){
			if ((*r_begin) != NULL){
				buffer = *r_begin;
				break;
			}
			r_begin++;
		}

		Rec->group_msg_list.erase(r_begin);
		Rec->group_msg_list.push_front(NULL);
		pthread_mutex_unlock(&Rec->m_send_group_mutex);

		std::cout<<"fuck   group..."<<std::endl;

		std::string group_name = ((*buffer).begin())->first;
		std::map<std::string, std::string>* buffer_msg = ((*buffer).begin())->second;

		std::string query_str = "select * from " + group_name + ";";
		const char* query = query_str.c_str();
		std::set<std::string> Members;
		int ret = Rec->Mysql_query(query);
		if (ret != -1 && ret != -2){
			while (Rec->mysql_row = mysql_fetch_row(Rec->mysql_res)){   //�õ�Ⱥ�����еĳ�Ա
				std::string member(Rec->mysql_row[0]);
				//����鿴�ǲ�������
				Members.insert(member);
			}
			if (Members.empty()){
				std::cout << "failed to get members..." << std::endl;
			}
			else{
				std::map<int, std::string>::iterator begin = Rec->all_fds.begin();
				std::map<int, std::string>::iterator end = Rec->all_fds.end();
				int request_id = -1;
				while (begin != end){
					if (Members.count((*begin).second)){    //�����Ա����
						int send_to_id = (*begin).first;
						//���Լ���Ϣ���е����ݰ�������һ��
						std::map<std::string, std::string>::iterator begin_msg = buffer_msg->begin();
						std::map<std::string, std::string>::iterator end_msg = buffer_msg->end();
						while (begin_msg != end_msg){
							int w = write(send_to_id, ((*begin_msg).second).c_str(), ((*begin_msg).second).size());
							if (w == -1){
								if (errno == EWOULDBLOCK){
									sleep(1);
									continue;
								}
								else{
									std::cout << "send to " << (*begin_msg).first << " failed" << std::endl;
								}
							}
							begin_msg++;
						}
					}
					begin++;
				}

				//������ɣ��ͷ��ڴ�
				delete buffer;
				delete buffer_msg;
			}
		}
		else{
			std::cout << group_name << " message failed to send..." << std::endl;
		}

	}
	return NULL;
}
