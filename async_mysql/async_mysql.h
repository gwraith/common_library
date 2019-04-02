/************************************************************************/
/* 异步mysql模块                                                         */
/************************************************************************/
#ifndef __ASYNC_MYSQL_H__
#define __ASYNC_MYSQL_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <tuple>
#include <list>
#include <map>
#include <memory>
#include <functional>

#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>
#include <my_getopt.h>

#include <event2/event.h>

#define MYSQL_REAL_CONNECT_START 0
#define MYSQL_REAL_CONNECT_CONT  1
#define MYSQL_REAL_CONNECT_FINAL 2

#define MYSQL_REAL_QUERY_START   10
#define MYSQL_REAL_QUERY_CONT    11
#define MYSQL_REAL_QUERY_FINAL   12
#define MYSQL_USE_RESULT         13

#define MYSQL_FETCH_ROW_START    20
#define MYSQL_FETCH_ROW_CONT     21
#define MYSQL_FETCH_ROW_FINAL    22

#define MYSQL_FREE_RESULT_START  23
#define MYSQL_FREE_RESULT_CONT   24
#define MYSQL_FREE_RESULT_FINAL  25

#define MYSQL_CLOSE_START  30
#define MYSQL_CLOSE_CONT   31
#define MYSQL_CLOSE_FREE   32

#define MYSQL_QUERY_ONCE_END  40

#define MYSQL_PING_START      41
#define MYSQL_PING_CONT       42
#define MYSQL_PING_FINAL      43

#define __DEBUG__

#ifdef __DEBUG__
class CAsyncMysql;
typedef struct _task_ctx_t_
{
    struct event *sigint_event;
    struct event *sigterm_event;
    struct event *sighup_event;
    struct event *worker_event;

    std::shared_ptr<CAsyncMysql> asyncmysql_ptr_;
}task_ctx_t;
#endif

using DealSelectCallback = std::function<void(char**, int, void *)>;
using DealUpdateCallback = std::function<void(int64_t, void *)>;
using DealErrorCallback  = std::function<void(int64_t, void *)>;

/* <mysql语句，是否需要结果，任务ID> */
typedef std::tuple<std::string, int, int64_t> QueryTuple_t;

typedef struct _state_data_
{
    int ST;                                /* 任务状态流转标志 */
    struct event_base *base;
    struct event *ev_mysql;
    MYSQL *mysql;
    MYSQL_RES *result;
    MYSQL *ret;
    int err;
    MYSQL_ROW row;

    std::string querystring;               /* 常驻SQL语句，一般用于从数据库获取任务 */
    std::list<QueryTuple_t> query_list;    /* 保存update类型的SQL语句，用于更新数据库任务信息 */

    std::string current_query;             /* 保存当前指向的SQL语句 */
    int current_need_result;               /* 当前语句是否要获取mysql执行的结果 */
    int64_t current_taskid;                /* 当前SQL语句的任务ID */

    _state_data_()
    {
        ST = MYSQL_REAL_CONNECT_START;
        result = nullptr;
        ret = nullptr;
        base = nullptr;
        current_need_result = -1;
        current_taskid = -1;
    };
}state_data;

class CAsyncMysql
{
public:
    CAsyncMysql(std::string host, std::string user, std::string pass, int port) : db_host(host), db_user(user), db_pass(pass), db_port(port)
    {
        sd = new state_data;
        sd->mysql = mysql_init(NULL);
        mysql_options(sd->mysql, MYSQL_OPT_NONBLOCK, 0);
        mysql_options(sd->mysql, MYSQL_SET_CHARSET_NAME, "utf8");
    }
    ~CAsyncMysql() {
        if(sd) delete sd;
    }

public:
    void init_async_mysql(struct event_base *, std::string, void *);
    void free_async_mysql(void *);

    void async_excute_query(std::string, int64_t);

    void registerHandler(DealSelectCallback c1, DealUpdateCallback c2, DealErrorCallback c3)
    {
        after_select_cb_ = c1;
        after_update_cb_  = c2;
        deal_error_cb_   = c3;
    }

private:
    std::string db_host;
    std::string db_user;
    std::string db_pass;
    int         db_port;

    state_data *sd;
    DealSelectCallback  after_select_cb_;
    DealUpdateCallback  after_update_cb_;
    DealErrorCallback   deal_error_cb_;

    static int mysql_status(short);
    static void async_next_event(int, int, void *);
    static void state_machine_handler(evutil_socket_t, short, void *);
};

#endif

