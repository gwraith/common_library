#include "send_email.h"

bool CSendEmail::send_email(std::string mail_from, std::string mail_to, std::string mail_subject, std::string mail_body)
{
    auto m_ctx = alloc_email_ctx(mail_from, mail_to, mail_subject, mail_body);
    if (m_ctx)
    {
        auto base = event_base_new();
        auto bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
        struct timeval timeout = { 10, 0 };

        bufferevent_set_timeouts(bev, &timeout, &timeout);
        bufferevent_setcb(bev, do_read_cb, NULL, do_event_cb, m_ctx);
        bufferevent_enable(bev, EV_READ | EV_WRITE);
        if (bufferevent_socket_connect_hostname(bev, NULL, AF_INET, mail_svr.c_str(), mail_port) < 0)
        {
            bufferevent_free(bev);
        }

        event_base_dispatch(base);
        event_base_free(base);

        auto res = m_ctx->result;
        delete m_ctx;
        if (res == 1) return true;
    }
    return false;
}

bool CSendEmail::send_email_ssl(std::string mail_from, std::string mail_to, std::string mail_subject, std::string mail_body)
{
    SSL_library_init();
    ERR_load_crypto_strings();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    SSL_CTX *ssl_ctx = NULL;
    SSL *ssl = NULL;
    sendemail_ctx_t *m_ctx = NULL;
    struct event_base *base = NULL;
    struct bufferevent *bev = NULL;
    struct timeval timeout = { 10, 0 };
    int ret = -1;

    ssl_ctx = SSL_CTX_new(SSLv23_method());
    if (!ssl_ctx) goto cleanup;
    ssl = SSL_new(ssl_ctx);
    if (!ssl) goto cleanup;
    m_ctx = alloc_email_ctx(mail_from, mail_to, mail_subject, mail_body);
    if (!m_ctx) goto cleanup;

    base = event_base_new();
    bev = bufferevent_openssl_socket_new(base, -1, ssl, BUFFEREVENT_SSL_CONNECTING, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
    bufferevent_set_timeouts(bev, &timeout, &timeout);
    bufferevent_setcb(bev, do_read_cb, NULL, do_event_cb, m_ctx);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
    if (bufferevent_socket_connect_hostname(bev, NULL, AF_INET, mail_svr.c_str(), mail_port) < 0)
    {
        bufferevent_free(bev);
    }

    event_base_dispatch(base);
    event_base_free(base);
    ret = m_ctx->result;

cleanup:
    if(m_ctx) delete m_ctx;
    if(ssl_ctx) SSL_CTX_free(ssl_ctx);
    EVP_cleanup();
    ERR_free_strings();
    ERR_remove_state(0);
    CRYPTO_cleanup_all_ex_data();
    sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
    if (ret == 1) return true;
    return false;
}

sendemail_ctx_t * CSendEmail::alloc_email_ctx(std::string from, std::string to, std::string subject, std::string body)
{
    sendemail_ctx_t *m_ctx = new sendemail_ctx_t;
    if (!m_ctx)
    {
        m_ctx->desc = "Alloc Email ctx Error.";
        return NULL;
    }
    m_ctx->mail_user = mail_user;
    m_ctx->mail_pass = mail_pass;
    m_ctx->mail_from = from;
    m_ctx->mail_to = to;
    m_ctx->mail_subject = subject;
    m_ctx->mail_body = body;

    char tmpbuff[2048] = { '\0' };
    strcpy(tmpbuff, to.c_str());
    char seps[] = " ,\t\n\r";
    char *token = NULL;
    char *ptr = NULL;
    token = strtok_r(tmpbuff, seps, &ptr);
    while (token != NULL)
    {
        m_ctx->recv_list.push_back(token);
        token = strtok_r(NULL, seps, &ptr);
    }

    return m_ctx;
}

void CSendEmail::do_read_cb(struct bufferevent *bev, void *ctx)
{
    auto m_ctx = (sendemail_ctx_t *)ctx;
    auto input = bufferevent_get_input(bev);
    auto output = bufferevent_get_output(bev);

    size_t n_read_out = 0;
    char *request_line = evbuffer_readln(input, &n_read_out, EVBUFFER_EOL_CRLF);
    if (!request_line) return;

    std::string response_line(request_line, n_read_out);
    auto rsp_code = stoi(std::string(request_line, 3));
    free(request_line);
    
    printf("request_line: %s\n", response_line.c_str());

    if (m_ctx->interaction_step == GET_SMTP_SERVER)
    {
        /* 220 smtp.aliyun-inc.com MX AliMail Server(10.147.42.135) */
        if (rsp_code != 220)
        {
            m_ctx->desc = "GET_SMTP_SERVER Error";
            bufferevent_free(bev);
        }
        m_ctx->interaction_step = SEND_HELLO_BUFFER;
        std::string tmpstr("EHLO localhost\r\n");
        evbuffer_add(output, tmpstr.c_str(), tmpstr.length());
    }
    else if (m_ctx->interaction_step == SEND_HELLO_BUFFER)
    {
        /* 250-smtp.aliyun-inc.com */
        if (rsp_code != 250)
        {
            m_ctx->desc = "SEND_HELLO_BUFFER Error.";
            bufferevent_free(bev);
        }
        while ((request_line = evbuffer_readln(input, &n_read_out, EVBUFFER_EOL_CRLF)) != NULL)
        {
            free(request_line);
        }
        m_ctx->interaction_step = SEND_AUTH_LOGIN;
        std::string tmpstr("AUTH LOGIN\r\n");
        evbuffer_add(output, tmpstr.c_str(), tmpstr.length());
    }
    else if (m_ctx->interaction_step == SEND_AUTH_LOGIN)
    {
        /* 334 dXNlcm5hbWU6 */
        if (rsp_code != 334)
        {
            m_ctx->desc = "SEND_AUTH_LOGIN Error.";
            bufferevent_free(bev);
        }
        m_ctx->interaction_step = SEND_LOGIN_USER;
        std::string tmpstr = string_format("%s\r\n", base64_encode(m_ctx->mail_user.c_str(), m_ctx->mail_user.length()).c_str());
        evbuffer_add(output, tmpstr.c_str(), tmpstr.length());
    }
    else if (m_ctx->interaction_step == SEND_LOGIN_USER)
    {
        /* 334 UGFzc3dvcmQ6 */
        if (rsp_code != 334)
        {
            m_ctx->desc = "SEND_LOGIN_USER Error";
            bufferevent_free(bev);
        }
        m_ctx->interaction_step = SEND_LOGIN_PASS;
        std::string tmpstr = string_format("%s\r\n", base64_encode(m_ctx->mail_pass.c_str(), m_ctx->mail_pass.length()).c_str());
        evbuffer_add(output, tmpstr.c_str(), tmpstr.length());
    }
    else if (m_ctx->interaction_step == SEND_LOGIN_PASS)
    {
        /* 235 Authentication successful */
        if (rsp_code != 235)
        {
            m_ctx->desc = "SEND_LOGIN_PASS Error";
            bufferevent_free(bev);
        }
        m_ctx->interaction_step = SEND_MAIL_FROM;
        std::string tmpstr = string_format("MAIL FROM: <%s>\r\n", m_ctx->mail_from.c_str());
        evbuffer_add(output, tmpstr.c_str(), tmpstr.length());
    }
    else if (m_ctx->interaction_step == SEND_MAIL_FROM)
    {
        /* 250 Mail Ok */
        if (rsp_code != 250)
        {
            m_ctx->desc = "SEND_MAIL_FROM Error.";
            bufferevent_free(bev);
        }
        m_ctx->interaction_step = SEND_MAIL_TO;
        std::string tmpstr = m_ctx->recv_list.front();
        m_ctx->recv_list.pop_front();
        tmpstr = string_format("RCPT TO: <%s>\r\n", tmpstr.c_str());
        evbuffer_add(output, tmpstr.c_str(), tmpstr.length());
    }
    else if (m_ctx->interaction_step == SEND_MAIL_TO)
    {
        /* 250 Rcpt Ok */
        if (rsp_code != 250)
        {
            m_ctx->desc = "SEND_MAIL_TO Error.";
            bufferevent_free(bev);
        }
        if (m_ctx->recv_list.size() > 0)
        {
            std::string tmpstr = m_ctx->recv_list.front();
            m_ctx->recv_list.pop_front();
            tmpstr = string_format("RCPT TO: <%s>\r\n", tmpstr.c_str());
            evbuffer_add(output, tmpstr.c_str(), tmpstr.length());
        }
        else
        {
            m_ctx->interaction_step = SEND_DATA_MESSAGE;
            std::string tmpstr("DATA\r\n");
            evbuffer_add(output, tmpstr.c_str(), tmpstr.length());
        }
    }
    else if (m_ctx->interaction_step == SEND_DATA_MESSAGE)
    {
        /* 354 End data with <CR><LF>.<CR><LF> */
        if (rsp_code != 354)
        {
            m_ctx->desc = "SEND_DATA_MESSAGE Error.";
            bufferevent_free(bev);
        }
        m_ctx->interaction_step = SEND_EMAIL_BODY;
        std::string tmpstr;
        tmpstr += string_format("From: %s\r\n", m_ctx->mail_from.c_str());
        tmpstr += string_format("To: %s\r\n", m_ctx->mail_to.c_str());
        tmpstr += string_format("Subject: %s\r\n", m_ctx->mail_subject.c_str());
        char date_str[100] = { '\0' };
        time_t timep = system_clock::to_time_t(system_clock::now());
        struct tm m_tnow = { 0 };
        localtime_r(&timep, &m_tnow);
        strftime(date_str, 100, "%a, %d %b %y %H:%M:%S +0800", &m_tnow);
        tmpstr += string_format("Date: %s\r\n", date_str);

        tmpstr += string_format("MIME-Version: 1.0\r\nContent-Type: multipart/mixed; boundary=%s\r\n\r\n", "gaoyaming1234");
        tmpstr += string_format("--%s\r\n", "gaoyaming1234");
        tmpstr += std::string("Content-Type: text/html; charset=UTF-8\r\nContent-Transfer-Encoding: 8bit\r\n\r\n");
        tmpstr += string_format("%s\r\n\r\n", m_ctx->mail_body.c_str());
        tmpstr += string_format("\r\n--%s--\r\n.\r\n", "gaoyaming1234");

        printf("%s", tmpstr.c_str());
        evbuffer_add(output, tmpstr.c_str(), tmpstr.length());
    }
    else if (m_ctx->interaction_step == SEND_EMAIL_BODY)
    {
        /* 250 Data Ok: queued as freedom */
        if (rsp_code != 250)
        {
            m_ctx->desc = "SEND_EMAIL_BODY Error.";
            bufferevent_free(bev);
        }
        m_ctx->interaction_step = SEND_QUIT_MESSAGE;
        std::string tmpstr("QUIT\r\n");
        evbuffer_add(output, tmpstr.c_str(), tmpstr.length());
    }
    else if (m_ctx->interaction_step == SEND_QUIT_MESSAGE)
    {
        /* 221 Bye */
        m_ctx->result = 1;
        m_ctx->desc = "Send Mail OK.";
        bufferevent_free(bev);
    }
}

void CSendEmail::do_event_cb(struct bufferevent *bev, short events, void *ctx)
{
    auto m_ctx = (sendemail_ctx_t *)ctx;
    if (events & BEV_EVENT_EOF)
    {
        m_ctx->desc = "Closed By Server.";
        bufferevent_free(bev);
    }
    else if (events & BEV_EVENT_TIMEOUT)
    {
        m_ctx->desc = "Communication Timeout";
        bufferevent_free(bev);
    }
    else if (events & BEV_EVENT_ERROR)
    {
        m_ctx->desc = "Some Error Happened.";
        bufferevent_free(bev);
    }
}

std::string CSendEmail::base64_encode(const char *in_buffer, int in_len)
{
    BIO *b64 = nullptr;
    BIO *bio = nullptr;
    BUF_MEM *bptr = nullptr;

    if (in_buffer == NULL) return "";

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_write(bio, in_buffer, in_len);
    BIO_flush(bio);

    BIO_get_mem_ptr(bio, &bptr);
    std::string out_buffer(bptr->data, bptr->length - 1);
    BIO_free_all(bio);
    return out_buffer;
}

int main()
{
    std::shared_ptr<CSendEmail> sendemail = std::make_shared<CSendEmail>("smtp.mxhichina.com", 25, "sender@test.com", "passwd");
    if (sendemail->send_email("sender@test.com", "recver1@test.com,recver2@test.com", "测试邮件发送", "this is a test email.<br>这是一封测试邮件，来自send_email."))
    {
        printf("send email OK.\n");
    }

    std::shared_ptr<CSendEmail> sendemail_ssl = std::make_shared<CSendEmail>("smtp.mxhichina.com", 465, "sender@test.com", "passwd");
    if (sendemail_ssl->send_email_ssl("sender@test.com", "recver1@test.com,recver2@test.com", "测试邮件发送", "this is a test email.<br>这是一封测试邮件，来自send_email_ssl."))
    {
        printf("send ssl email OK.\n");
    }
    return 0;
}
