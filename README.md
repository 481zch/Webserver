总共会有下面这几大模块
缓冲区
http的处理
webserver
线程池
对象池
连接池
日志

对于高并发模式：支持reactor和proactor
reactor只能采用epoll + 非阻塞 + 边沿触发
proactor采用AIO作为IO多路复用机制

对于数据库，考虑使用MYSQL作为主存储数据库，
使用redis来优化查询效率

环形缓冲区还有一大优势就是可以避免reset的次数

对于最后的测试，使用GTEST和webrench来完成