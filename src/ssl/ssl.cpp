#include "ssl/ssl.h"

SSLConnection::SSLConnection(SSL_CTX *ctx, int clientSocket):
    m_ctx(ctx), m_clientSocket(clientSocket), m_ssl(nullptr)
{}

SSLConnection::~SSLConnection()
{
    close();
}

bool SSLConnection::init()
{
    m_ssl = SSL_new(m_ctx);
    if (!m_ssl) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    SSL_set_fd(m_ssl, m_clientSocket);

    // SSL握手
    if (SSL_accept(m_ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(m_ssl);
        m_ssl = nullptr;
        return false;
    }

    return true;
}

void SSLConnection::close()
{
    if (m_ssl) {
        SSL_shutdown(m_ssl);
        SSL_free(m_ssl);
        m_ssl = nullptr;
    }
    ::close(m_clientSocket);
}