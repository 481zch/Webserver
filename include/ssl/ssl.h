#pragma once
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include <string>

class SSLConnection {
public:
    SSLConnection(SSL_CTX* ctx, int clientSocket);
    ~SSLConnection();

    bool init();

    ssize_t read(char* buffer, size_t size);
    ssize_t write(const char* buffer, size_t size);

    void close();

private:
    SSL* m_ssl;
    int m_clientSocket;
    SSL_CTX* m_ctx;
};