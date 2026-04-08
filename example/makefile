all: wsserver json mysql

mysql: mysql.c
	gcc $^ -o $@ -L/usr/lib64/mysql   -lmysqlclient

wsserver: wsserver.cpp
	g++ -std=c++11 -I/usr/local/include -I/usr/include/boost wsserver.cpp -o wsserver -lpthread -lboost_system

json: json.cpp
	g++ -std=c++11 -I/usr/local/include -I/usr/include/boost json.cpp -o json -lpthread -lboost_system -ljsoncpp

clean:
	rm -f wsserver json
