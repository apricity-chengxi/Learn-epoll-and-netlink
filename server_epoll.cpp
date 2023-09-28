#include <cstdio>
#include <ctype.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <iostream>
#include <cstring>
#include <arpa/inet.h>
using namespace std;

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cout << "Using: ./server 5005" << endl;
        return -1;
    }

    // 创建监听的套接字
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        perror("socket");
        return -1;
    }

    // 绑定
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(argv[1]));
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // 设置端口复用
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定端口
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        perror("bind");
        close(listenfd);
        return -1;
    }

    // 监听
    if (listen(listenfd, 64) != 0)
    {
        perror("listen");
        close(listenfd);
        return -1;
    }

    // 现在只有监听的文档描述符
    // 所有的文档描述符对应读写缓冲区状态都是委托内核进行检测的epoll
    // 创建一个epoll模型
    int epfd = epoll_create(100);
    if (epfd == -1)
    {
        perror("epoll_create");
        return -1;
    }

    // 往epoll实例中添加需要检测的节点, 现在只有监听的文档描述符
    struct epoll_event ev;
    ev.events = EPOLLIN; // 检测lfd读读缓冲区是否有数据
    ev.data.fd = listenfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) == -1)
    {
        perror("epoll_ctl");
        return -1;
    }

    struct epoll_event evs[1024];
    int size = sizeof(evs) / sizeof(struct epoll_event);

    while (true)
    {
        // 调用一次, 检测一次
        int num = epoll_wait(epfd, evs, size, -1);
        for (int i = 0; i < num; ++i)
        {
            // 取出当前的文档描述符
            int curfd = evs[i].data.fd;
            // 判断这个文档描述符是不是用于监听的
            if (curfd == listenfd)
            {
                // 创建新的连接
                int cfd = accept(curfd, NULL, NULL);
                // 新得到的文档描述符添加到epoll模型中, 下一轮循环的时候就可以被检测了
                ev.events = EPOLLIN; // 读缓冲区是否有数据
                ev.data.fd = cfd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev) == -1)
                {
                    perror("epoll_ctl-accept");
                    exit(0);
                }
            }
            else
            {
                // 处理通信的文档描述符
                // 接收数据
                char buf[1024];
                memset(buf, 0, sizeof(buf));
                int len = recv(curfd, buf, sizeof(buf), 0);
                if (len == 0)
                {
                    printf("客户端已经断开了连接\n");
                    // 将这个文档描述符从epoll模型中删除
                    epoll_ctl(epfd, EPOLL_CTL_DEL, curfd, NULL);
                    close(curfd);
                    return -1;
                }
                else if (len > 0)
                {
                    printf("客户端say: %s\n", buf);
                    send(curfd, buf, len, 0);
                }
                else
                {
                    perror("recv");
                    return -1;
                }
            }
        }
    }
}