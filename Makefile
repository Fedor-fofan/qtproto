server:
	g++ -Icommon src/server.cpp common/NetworkUtils.cpp -o server -lws2_32