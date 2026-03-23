wsserver:wsserver.cpp
	g++ -std=c++11 -I/usr/local/include -I/usr/include/boost wsserver.cpp -o wsserver -lpthread -lboost_system

clean:
	rm -f wsserver
