#include "MyServer.h"

MyRector my_rec;

void perr_exit(int signo){
	std::cout << "program error signal: " << signo << std::endl;
	my_rec.Uninit();
}

void daemon_run(){
	pid_t pid;
	signal(SIGCHLD, SIG_IGN);

	pid = fork();
	if (pid < 0){
		std::cout << "fock error" << std::endl;
		exit(-1);
	}
	else if (pid > 0){
		exit(0);
	}

	setsid();
	int fd = open("/dev/null",O_RDWR, 0);
	if (fd != -1){
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
	}
	if (fd > 2)  //关闭指向dev/null文件描述符
		close(fd);
}

int main(int argc, char* argv[]){
	signal(SIGCHLD, SIG_DFL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, perr_exit);
	signal(SIGKILL, perr_exit);
	signal(SIGTERM, perr_exit);

	int port = 0;
	int ch;
	bool bdaemon = false;

	while ((ch = getopt(argc, argv, "p:d")) != -1)
	{
		switch (ch)
		{
		case 'd':  
			bdaemon = true;
			break;
		case 'p':    //如果p后面带参数了
			port = atol(optarg);
			break;
		}
	}


	if (bdaemon)
		daemon_run();


	if (port == 0)
		port = 12345;

	if (!my_rec.Init("127.0.0.1", 12345))
		return -1;


	my_rec.main_loop(&my_rec);

	std::cout << "main exit" << std::endl;

	return 0;

};
