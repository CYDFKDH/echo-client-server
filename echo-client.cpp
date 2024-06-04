#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <thread>

void myerror(const char* msg) {
    fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno);
}

void usage() {
    printf("syntax: echo-client <ip> <port>\n");
    printf("sample: echo-client 127.0.0.1 1234\n");
}

struct Param {
    char* ip{nullptr};
    char* port{nullptr};

    bool parse(int argc, char* argv[]) {
        if (argc == 3) {
            ip = argv[1];
            port = argv[2];
        }
        return (ip != nullptr) && (port != nullptr);
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
    }
    printf("disconnected\n");
    fflush(stdout);
    ::close(sd);
    exit(0);
}

int main(int argc, char* argv[]) {
    if (!param.parse(argc, argv)) {
        usage();
        return -1;
    }

    struct addrinfo aiInput, *aiOutput, *ai;
    memset(&aiInput, 0, sizeof(aiInput));
    aiInput.ai_family = AF_INET;
    aiInput.ai_socktype = SOCK_STREAM;
    aiInput.ai_flags = 0;
    aiInput.ai_protocol = 0;

    int res = getaddrinfo(param.ip, param.port, &aiInput, &aiOutput);
    if (res != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res));
        return -1;
    }

    int sd;
    for (ai = aiOutput; ai != nullptr; ai = ai->ai_next) {
        sd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sd != -1) break;
    }
    if (ai == nullptr) {
        fprintf(stderr, "can not find socket for %s\n", param.ip);
        return -1;
    }

    {
        int optval = 1;
        int res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        if (res == -1) {
            myerror("setsockopt");
            return -1;
        }
    }

    {
        int res = ::connect(sd, ai->ai_addr, ai->ai_addrlen);
        if (res == -1) {
            myerror("connect");
            return -1;
        }
    }

    std::thread t(recvThread, sd);
    t.detach();

    while (true) {
        std::string s;
        std::getline(std::cin, s);
        s += "\r\n";
        ssize_t res = ::send(sd, s.data(), s.size(), 0);
        if (res == 0 || res == -1) {
            fprintf(stderr, "send return %zd", res);
            myerror(" ");
            break;
        }
    }
    ::close(sd);
}

