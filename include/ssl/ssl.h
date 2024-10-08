#pragma once
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include <string>
#include "buffer/buffer.h"

class SSLServer {
public:
    SSLServer(const char* certFile, const char* keyFile);
    ~SSLServer();
    bool init();
    SSL_CTX* getCtx() const {return m_ctx;};
    bool SSLGetConnection(int clientSocket, SSL* &ssl);

private:
    SSL_CTX* m_ctx;
    const char* m_certFile;
    const char* m_keyFile;
};