#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
using namespace std;

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cout << "Using: ./server 5005" << endl;
        return -1;
    }

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(atoi(argv[1]));

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        perror("bind");
        close(listenfd);
        return -1;
    }

    if (listen(listenfd, 5) != 0)
    {
        perror("listen");
        close(listenfd);
        return -1;
    }

    int clientfd = accept(listenfd, 0, 0);
    if (clientfd == -1)
    {
        perror("accept");
        close(listenfd);
        return -1;
    }

    cout << "客户端已连接" << endl;

    char buffer[1024];
    while (true)
    {
        int iret;
        memset(buffer, 0, sizeof(buffer));
        if ((iret = recv(clientfd, buffer, sizeof(buffer), 0)) <= 0)
        {
            cout << "iret=" << iret << endl;
            break;
        }
        cout << "接受：" << buffer << endl;

        strcpy(buffer, "ok");
        if ((iret = send(clientfd, buffer, strlen(buffer), 0)) <= 0)
        {
            perror("send");
            break;
        }
        cout << "发送:" << buffer << endl;
    }

    close(listenfd);
    close(clientfd);
}