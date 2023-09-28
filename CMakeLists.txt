cmake_minimum_required(VERSION 3.12)
project(net)

add_executable(client client.cpp)
add_executable(netlink netlink.cpp)
add_executable(server server.cpp)
add_executable(server_epoll server_epoll.cpp)
add_executable(server_epoll_netlink server_epoll_netlink.cpp)