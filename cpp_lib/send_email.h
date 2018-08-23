#ifndef __SEND_EMAIL_HEADER_H__
#define __SEND_EMAIL_HEADER_H__
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <string>
#include <memory>
#include <list>
#include <chrono>

#include <event2/event.h>
#include <event2/util.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/bufferevent_ssl.h>

#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "string_format.h"

#define GET_SMTP_SERVER     0
#define SEND_HELLO_BUFFER   1
#define SEND_AUTH_LOGIN     2
#define SEND_LOGIN_USER     3
#define SEND_LOGIN_PASS     4
#define SEND_MAIL_FROM      5
#define SEND_MAIL_TO        6
#define SEND_DATA_MESSAGE   7
#define SEND_EMAIL_BODY     8
#define SEND_QUIT_MESSAGE   9

using namespace std;
using namespace chrono;

typedef struct _sendemail_ctx_
{
    std::string mail_user;
    std::string mail_pass;

    std::string mail_from;
    std::string mail_to;

    std::string mail_subject;
    std::string mail_body;
    
    std::list<std::string> recv_list;

    int interaction_step;

    int result;
    std::string desc;

    _sendemail_ctx_()
    {
        interaction_step = GET_SMTP_SERVER;
        result = -1;
        desc = "Mail Fail.";
    };
}sendemail_ctx_t;

class CSendEmail
{
public:
    //************************************
    // 函数参数:  std::string svr       邮件服务器
    // 函数参数:  int port              邮件服务器端口
    // 函数参数:  std::string user      登录用户
    // 函数参数:  std::string passwd    登陆密码
    //************************************
    CSendEmail(std::string svr, int port, std::string user, std::string passwd) : mail_svr(std::move(svr)), mail_port(port), mail_user(std::move(user)), mail_pass(std::move(passwd)) {}
    ~CSendEmail() = default;

    //************************************
    // 函数参数:  std::string mail_from         发件人
    // 函数参数:  std::string mail_to           收件人
    // 函数参数:  std::string mail_subject      邮件主题
    // 函数参数:  std::string mail_body         邮件内容
    //************************************
    bool send_email(std::string mail_from, std::string mail_to, std::string mail_subject, std::string mail_body);
    bool send_email_ssl(std::string mail_from, std::string mail_to, std::string mail_subject, std::string mail_body);

private:
    sendemail_ctx_t *alloc_email_ctx(std::string from, std::string to, std::string subject, std::string body);
    static void do_read_cb(struct bufferevent *bev, void *ctx);
    static void do_event_cb(struct bufferevent *bev, short events, void *ctx);
    static std::string base64_encode(const char *in_buffer, int in_len);
    
private:
    std::string mail_svr;
    int mail_port;
    std::string mail_user;
    std::string mail_pass;
};

#endif
