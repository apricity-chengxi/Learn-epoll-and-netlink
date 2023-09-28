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
#include <asm/types.h>
#include <sys/stat.h>
#include <linux/netlink.h>
#include <linux/socket.h>
using namespace std;

#define NETLINK_UPDATE_DOMAIN 30
#define MAX_DOMAIN 16
#define MAX_PLOAD 125

/*
netlink端口号 12306
协议类型 30
*/

struct mess
{
    int mess_id;             // 表示此次传输动作，0表示entry创建、1表示entry更新、2entry删除
    struct in_addr ip;       // 对应ip，如果是本机产生，那么置为-1
    int port;                // 表示对应端口
    int domains[MAX_DOMAIN]; // 表示对应读过的domains，初始置为-1
    char size_zero[32];      // 不使用,为了和内核结构体一样大小
};

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cout << "Using: ./server 5005" << endl;
        return -1;
    }

    // 创建监听的套接字
    int listenfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_UPDATE_DOMAIN);
    if (listenfd == -1)
    {
        perror("socket");
        return -1;
    }

    // 绑定
    struct sockaddr_nl servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.nl_family = AF_NETLINK;
    servaddr.nl_pid = htons(atoi(argv[1])); // 端口号
    servaddr.nl_groups = 0;
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        perror("bind() error");
        close(listenfd);
        return -1;
    }

    struct sockaddr_nl daddr;
    memset(&daddr, 0, sizeof(daddr));
    daddr.nl_family = AF_NETLINK;
    daddr.nl_pid = 0; // to kernel
    daddr.nl_groups = 0;

    struct nlmsghdr *nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PLOAD));
    memset(nlh, 0, sizeof(struct nlmsghdr));
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PLOAD);
    nlh->nlmsg_flags = 0;
    nlh->nlmsg_type = 0;
    nlh->nlmsg_seq = 0;
    nlh->nlmsg_pid = servaddr.nl_pid; //self port


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

    mess msg;
    struct epoll_event evs[1024];
    int size = sizeof(evs) / sizeof(struct epoll_event);
    socklen_t sklen = sizeof(struct sockaddr_nl);

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
                    perror("epoll_ctl-accept error");
                    exit(0);
                }
            }
            else
            {
                // 处理通信的文档描述符
                // 接收数据
                memset(&msg, 0, sizeof(msg));
                int len = recvfrom(curfd, &msg, sizeof(mess), 0, (struct sockaddr *)&daddr, &sklen);
                if (len == 0)
                {
                    printf("客户端已经断开了连接\n");
                    // 将这个文档描述符从epoll模型中删除
                    epoll_ctl(epfd, EPOLL_CTL_DEL, curfd, NULL);
                    close(curfd);
                }
                else if (len > 0)
                {
                    printf("kernel say: mess_id = %d\n", msg.mess_id);
                    printf("kernel say: ip = %d\n", msg.ip.s_addr);
                    printf("kernel say: port = %d\n", msg.port);
                    // TODO:发送给master数据
                }
                else
                {
                    perror("recvfrom error");
                    return -1;
                }
            }
        }
    }
}


// char *umsg = "hello netlink!!";
// memcpy(NLMSG_DATA(nlh), umsg, strlen(umsg));
// ret = sendto(skfd, nlh, nlh->nlmsg_len, 0, (struct sockaddr *)&daddr, sizeof(struct sockaddr_nl));
// if (!ret)
// {
//     perror("sendto error\n");
//     close(skfd);
//     exit(-1);
// }
// printf("send kernel:%s\n", umsg);

// memset(&u_info, 0, sizeof(u_info));
// len = sizeof(struct sockaddr_nl);
// ret = recvfrom(skfd, &u_info, sizeof(user_msg_info), 0, (struct sockaddr *)&daddr, &len);
// if (!ret)
// {
//     perror("recv form kernel error\n");
//     close(skfd);
//     exit(-1);
// }

// printf("from kernel:%s\n", u_info.msg);
// close(skfd);

// free((void *)nlh);