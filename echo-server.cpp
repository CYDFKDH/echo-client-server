#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
#include <vector>

using namespace std;

vector<int> cli_List;

void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }

void usage() {
	printf("syntax: echo-server <port> [-e[-b]]\n");
	printf("  -e : echo\n");
	printf("  -b : broadcast\n");
	printf("sample: echo-server 1234\n");
}

struct Param {
	bool echo{false};
	bool broadcast{false};
	uint16_t port{0};

	bool parse(int argc, char* argv[]) {
		if (2 <= argc || argc <= 4) {
			port = atoi(argv[1]);
			
			if (3 <= argc && strcmp(argv[2], "-e") == 0) {
				echo = true;
				if (argc == 4 && strcmp(argv[3], "-b") == 0) {
					broadcast = true;
				}
			}

		}
		return port != 0;
	}
} param;

void recvThread(int sd) {
	printf("connected\n");
	fflush(stdout);
	static const int BUFSIZE = 65536;
	char buf[BUFSIZE];
	while (true) {
		ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
		if (res == 0 || res == -1) {
			fprintf(stderr, "recv return %zd", res);
			myerror(" ");
			break;
		}
		buf[res] = '\0';
		printf("%s", buf);
		fflush(stdout);
		if (param.echo) {
			if(param.broadcast){
				for(int i=0 ; i < cli_List.size() ; i++){
					res = ::send(cli_List[i], buf, res, 0);
					if (res == 0 || res == -1) {
						fprintf(stderr, "send return %zd", res);
						perror(" ");
						break;
					}
				}
			}else{
				res = ::send(sd, buf, res, 0);
				if (res == 0 || res == -1) {
					fprintf(stderr, "send return %zd", res);
					myerror(" ");
					break;
				}
			}
		}
	}
	printf("disconnected\n");
	fflush(stdout);
	
	for(int i=0 ; i < cli_List.size() ; i++){
		if(cli_List[i] == sd){
			cli_List.erase(cli_List.begin()+i);
			break;
		}
	}
	
	::close(sd);
}

int main(int argc, char* argv[]) {
	if (!param.parse(argc, argv)) {
		usage();
		return -1;
	}

	int sd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		myerror("socket");
		return -1;
	}

	
	int optval = 1;
	int result = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (result == -1) {
		myerror("setsockopt");
		return -1;
	}
	

	
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(param.port);

	ssize_t res = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
	if (res == -1) {
		myerror("bind");
		return -1;
	}


	result = listen(sd, 5);
	if (result == -1) {
		myerror("listen");
		return -1;
	}

	while (true) {
		struct sockaddr_in addr;
		socklen_t len = sizeof(addr);
		int newsd = ::accept(sd, (struct sockaddr *)&addr, &len);
		if (newsd == -1) {
			myerror("accept");
			break;
		}
		cli_List.push_back(newsd);
		std::thread* t = new std::thread(recvThread, newsd);
		t->detach();
	}
	::close(sd);
}
