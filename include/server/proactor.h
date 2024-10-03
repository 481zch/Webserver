#include <liburing.h>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#define QUEUE_DEPTH 256  // io_uring 队列深度
#define BUFFER_SIZE 1024  // 每次读写的缓冲区大小

struct io_uring ring;  // io_uring 实例

// ProactorServer类，使用liburing实现的Proactor模式服务器
class ProactorServer {
public:
    // 构造函数，初始化服务器并配置io_uring
    ProactorServer(int port) {
        server_fd = setupServer(port);  // 设置服务器并返回服务器文件描述符
        init_io_uring();  // 初始化 io_uring
    }

    // 析构函数，释放资源
    ~ProactorServer() {
        close(server_fd);  // 关闭服务器套接字
        io_uring_queue_exit(&ring);  // 退出io_uring队列
    }

    // 运行服务器主循环
    void run() {
        while (true) {
            acceptNewConnection();  // 接受新连接
            process_io_uring();  // 处理 io_uring 事件
        }
    }

private:
    int server_fd;  // 服务器文件描述符

    // 初始化 io_uring 队列
    void init_io_uring() {
        io_uring_queue_init(QUEUE_DEPTH, &ring, 0);  // 初始化 io_uring，指定队列深度
    }

    // 设置服务器，创建并绑定套接字，监听给定端口
    int setupServer(int port) {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);  // 创建套接字
        struct sockaddr_in addr;  // 服务器地址
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;  // 使用 IPv4
        addr.sin_addr.s_addr = INADDR_ANY;  // 监听所有可用接口
        addr.sin_port = htons(port);  // 设置端口号，使用网络字节序

        bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));  // 绑定服务器套接字到指定地址和端口
        listen(server_fd, 100);  // 开始监听，最多同时允许 100 个待处理连接
        return server_fd;
    }

    // 接受新客户端连接
    void acceptNewConnection() {
        struct sockaddr_in client_addr;  // 客户端地址
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);  // 接收客户端连接

        // 如果连接成功
        if (client_fd > 0) {
            std::cout << "Accepted connection from " << inet_ntoa(client_addr.sin_addr) << std::endl;
            submit_read_request(client_fd);  // 提交读请求以处理新连接的数据
        }
    }

    // 向 io_uring 提交读请求
    void submit_read_request(int fd) {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);  // 获取 io_uring 提交队列条目 (sqe)
        char* buffer = new char[BUFFER_SIZE];  // 为读取的数据分配缓冲区

        io_uring_prep_read(sqe, fd, buffer, BUFFER_SIZE, 0);  // 准备读请求
        io_uring_sqe_set_data(sqe, buffer);  // 将缓冲区数据传递给完成队列 (cqe)
        io_uring_submit(&ring);  // 提交到 io_uring
    }

    // 处理 io_uring 完成的 IO 操作
    void process_io_uring() {
        struct io_uring_cqe* cqe;  // 完成队列条目 (cqe)
        // 等待 cqe，并处理它们
        while (io_uring_wait_cqe(&ring, &cqe) == 0) {
            char* buffer = (char*)io_uring_cqe_get_data(cqe);  // 获取关联的缓冲区
            int fd = cqe->res;  // 获取文件描述符，表示IO操作完成的结果

            if (cqe->res > 0) {
                buffer[cqe->res] = '\0';  // 将读取的内容转为字符串
                std::cout << "Received data: " << buffer << std::endl;  // 打印接收到的数据
                submit_write_request(fd, buffer);  // 提交写请求以发送响应
            } else {
                std::cout << "Connection closed." << std::endl;
                close(fd);  // 关闭客户端连接
                delete[] buffer;  // 释放缓冲区
            }
            io_uring_cqe_seen(&ring, cqe);  // 标记此 cqe 为已处理
        }
    }

    // 提交写请求，将数据发送给客户端
    void submit_write_request(int fd, const char* response) {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);  // 获取提交队列条目

        io_uring_prep_write(sqe, fd, response, strlen(response), 0);  // 准备写请求
        io_uring_sqe_set_data(sqe, nullptr);  // 无需缓冲区数据
        io_uring_submit(&ring);  // 提交到 io_uring
    }
};

int main() {
    int port = 8080;  // 服务器监听的端口
    ProactorServer server(port);  // 创建 Proactor 服务器实例，监听指定端口

    std::cout << "Proactor-based server is running on port " << port << "..." << std::endl;

    server.run();  // 启动服务器的事件循环，接受客户端连接并处理IO请求

    return 0;
}
