all:
	g++ -g -Wall -std=c++11 -I /usr/local/mysql/include -L /usr/local/mysql/lib -o main main.cpp MyServer.cpp -I /home/adminwang/A_project/2019_6_2  -lpthread -lmysqlclient

clean:
	rm -rf main
