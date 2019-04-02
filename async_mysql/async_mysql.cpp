#include "async_mysql.h"

void CAsyncMysql::init_async_mysql(event_base *base, std::string query, void *arg)
{
    sd->base = base;
    sd->querystring = query;

    /* 插入常驻语句(获取执行结果，默认ID为 - 1) */
    sd->query_list.emplace_back(query, 1, -1);

    struct timeval work_sec = { 2, 0 };
    sd->ev_mysql = event_new(base, -1, EV_TIMEOUT, state_machine_handler, arg);
    event_add(sd->ev_mysql, &work_sec);
}

void CAsyncMysql::free_async_mysql(void *arg)
{
    event_del(sd->ev_mysql);
    async_next_event(MYSQL_CLOSE_START, MYSQL_WAIT_TIMEOUT, arg);
}


void CAsyncMysql::async_excute_query(std::string query, int64_t id)
{
    /* 异步插入等待执行的SQL语句默认不需要收集结果 */
    if(sd) sd->query_list.emplace_front(query, 0, id);
}

int CAsyncMysql::mysql_status(short events)
{
    int status = 0;
    if (events & EV_READ)
        status |= MYSQL_WAIT_READ;
    if (events & EV_WRITE)
        status |= MYSQL_WAIT_WRITE;
    if (events & EV_TIMEOUT)
        status |= MYSQL_WAIT_TIMEOUT;
    return status;
}

void CAsyncMysql::async_next_event(int new_st, int status, void *arg)
{
    task_ctx_t *m_ptr = (task_ctx_t *)arg;
    state_data *sd = m_ptr->asyncmysql_ptr_->sd;

    short wait_event = 0;
    struct timeval tv;
    struct timeval *ptv = NULL;
    int fd = -1;

    if (status & MYSQL_WAIT_READ) 
        wait_event = wait_event | EV_READ;
    if (status & MYSQL_WAIT_WRITE) 
        wait_event = wait_event | EV_WRITE;
    if (wait_event) 
        fd = mysql_get_socket(sd->mysql);

    if (status & MYSQL_WAIT_TIMEOUT)
    {
        tv.tv_sec = mysql_get_timeout_value(sd->mysql);
        if (new_st == MYSQL_REAL_QUERY_START) tv.tv_sec = 5;
        tv.tv_usec = 0;
        ptv = &tv;
        wait_event = wait_event | EV_TIMEOUT;
    }

    sd->ST = new_st;
    event_assign(sd->ev_mysql, sd->base, fd, wait_event, state_machine_handler, arg);
    event_add(sd->ev_mysql, ptv);
}

#define NEXT_STEP_IMMEDIATE(sd_, new_st_, fd, events, arg) do { sd_->ST = new_st_; state_machine_handler(fd, events, arg); } while (0)
void CAsyncMysql::state_machine_handler(evutil_socket_t fd, short events, void *arg)
{
    auto m_ptr = (task_ctx_t *)arg;
    auto as = m_ptr->asyncmysql_ptr_;
    auto sd = as->sd;

    /* Connect 阶段 */
    if (sd->ST == MYSQL_REAL_CONNECT_START)
    {
        auto status = mysql_real_connect_start(&sd->ret, sd->mysql, as->db_host.c_str(), as->db_user.c_str(), as->db_pass.c_str(), NULL, as->db_port, NULL, 0);
        if (status)
            async_next_event(MYSQL_REAL_CONNECT_CONT, status, arg);
        else
            NEXT_STEP_IMMEDIATE(sd, MYSQL_REAL_CONNECT_FINAL, fd, events, arg);
    }
    else if (sd->ST == MYSQL_REAL_CONNECT_CONT)
    {
        auto status = mysql_real_connect_cont(&sd->ret, sd->mysql, mysql_status(events));
        if (status)
            async_next_event(MYSQL_REAL_CONNECT_CONT, status, arg);
        else
            NEXT_STEP_IMMEDIATE(sd, MYSQL_REAL_CONNECT_FINAL, fd, events, arg);
    }
    else if (sd->ST == MYSQL_REAL_CONNECT_FINAL)
    {
        if (!sd->ret)
        {
            printf("connect mysql fail. err[%s]\n", mysql_error(sd->mysql));
            async_next_event(MYSQL_REAL_CONNECT_START, MYSQL_WAIT_TIMEOUT, arg);
        }
        else
            NEXT_STEP_IMMEDIATE(sd, MYSQL_REAL_QUERY_START, fd, events, arg);
    }
    /* Query 阶段 */
    else if (sd->ST == MYSQL_REAL_QUERY_START)
    {
        if (sd->query_list.empty())
        {
            /* 队列中没有其他SQL语句了，插入常驻语句(获取执行结果，默认ID为-1) 等待下个周期继续 */
            sd->query_list.emplace_back(sd->querystring, 1, -1);
            async_next_event(MYSQL_REAL_QUERY_START, MYSQL_WAIT_TIMEOUT, arg);
        }
        else
        {
            auto it = sd->query_list.front();
            sd->query_list.pop_front();

            sd->current_query = std::get<0>(it);
            sd->current_need_result = std::get<1>(it);
            sd->current_taskid = std::get<2>(it);

            auto status = mysql_real_query_start(&sd->err, sd->mysql, (char *)sd->current_query.c_str(), sd->current_query.length());
            printf("run query: %s\n", sd->current_query.c_str());
            if (status)
                async_next_event(MYSQL_REAL_QUERY_CONT, status, arg);
            else
                NEXT_STEP_IMMEDIATE(sd, MYSQL_REAL_QUERY_FINAL, fd, events, arg);
        }
    }
    else if (sd->ST == MYSQL_REAL_QUERY_CONT)
    {
        auto status = mysql_real_query_cont(&sd->err, sd->mysql, mysql_status(events));
        if (status)
            async_next_event(MYSQL_REAL_QUERY_CONT, status, arg);
        else
            NEXT_STEP_IMMEDIATE(sd, MYSQL_REAL_QUERY_FINAL, fd, events, arg);
    }
    /* Use Result 阶段 */
    else if (sd->ST == MYSQL_REAL_QUERY_FINAL)
    {
        if (sd->err)
        {
            printf("mysql query run fail. err[%s]\n", mysql_error(sd->mysql));
            /* SQL语句执行失败，判断连接是否存活 */
            if (sd->current_taskid != -1) 
            {
                as->deal_error_cb_(sd->current_taskid, arg);
            }
            NEXT_STEP_IMMEDIATE(sd, MYSQL_PING_START, fd, events, arg);
        }
        else
        {
            /* 根据不同的SQL语句判断是否需要获取结果 */
            if (sd->current_need_result)
            {
                NEXT_STEP_IMMEDIATE(sd, MYSQL_USE_RESULT, fd, events, arg);
            }
            else
            {
                as->after_update_cb_(sd->current_taskid, arg);
                NEXT_STEP_IMMEDIATE(sd, MYSQL_QUERY_ONCE_END, fd, events, arg);
            }
        }
    }
    else if (sd->ST == MYSQL_USE_RESULT)
    {
        sd->result = mysql_use_result(sd->mysql);
        if (!sd->result)
            NEXT_STEP_IMMEDIATE(sd, MYSQL_QUERY_ONCE_END, fd, events, arg);
        else
            NEXT_STEP_IMMEDIATE(sd, MYSQL_FETCH_ROW_START, fd, events, arg);
    }
    /* Fetch Row 阶段 */
    else if (sd->ST == MYSQL_FETCH_ROW_START)
    {
        auto status = mysql_fetch_row_start(&sd->row, sd->result);
        if (status)
            async_next_event(MYSQL_FETCH_ROW_CONT, status, arg);
        else
            NEXT_STEP_IMMEDIATE(sd, MYSQL_FETCH_ROW_FINAL, fd, events, arg);
    }
    else if (sd->ST == MYSQL_FETCH_ROW_CONT)
    {
        auto status = mysql_fetch_row_cont(&sd->row, sd->result, mysql_status(events));
        if (status)
            async_next_event(MYSQL_FETCH_ROW_CONT, status, arg);
        else
            NEXT_STEP_IMMEDIATE(sd, MYSQL_FETCH_ROW_FINAL, fd, events, arg);
    }
    else if (sd->ST == MYSQL_FETCH_ROW_FINAL)
    {
        if (sd->row)
        {
            as->after_select_cb_(sd->row, (int)mysql_num_fields(sd->result), arg);
            NEXT_STEP_IMMEDIATE(sd, MYSQL_FETCH_ROW_START, fd, events, arg);
        }
        else
        {
            /* 此处非必要，当result中数据都被读出时， mysql_free_result非阻塞*/
            NEXT_STEP_IMMEDIATE(sd, MYSQL_FREE_RESULT_START, fd, events, arg);
        }
    }
    /* free result阶段 */
    else if (sd->ST == MYSQL_FREE_RESULT_START)
    {
        auto status = mysql_free_result_start(sd->result);
        if (status)
            async_next_event(MYSQL_FREE_RESULT_CONT, status, arg);
        else
            NEXT_STEP_IMMEDIATE(sd, MYSQL_FREE_RESULT_FINAL, fd, events, arg);
    }
    else if (sd->ST == MYSQL_FREE_RESULT_CONT)
    {
        auto status = mysql_free_result_cont(sd->result, mysql_status(events));
        if (status)
            async_next_event(MYSQL_FREE_RESULT_CONT, status, arg);
        else
            NEXT_STEP_IMMEDIATE(sd, MYSQL_FREE_RESULT_FINAL, fd, events, arg);
    }
    else if (sd->ST == MYSQL_FREE_RESULT_FINAL)
    {
        NEXT_STEP_IMMEDIATE(sd, MYSQL_QUERY_ONCE_END, fd, events, arg);
    }
    else if (sd->ST == MYSQL_QUERY_ONCE_END)
    {
        NEXT_STEP_IMMEDIATE(sd, MYSQL_REAL_QUERY_START, fd, events, arg);
    }
    /* 判断连接是否存活 */
    else if (sd->ST == MYSQL_PING_START)
    {
        auto status = mysql_ping_start(&sd->err, sd->mysql);
        if (status)
            async_next_event(MYSQL_PING_CONT, status, arg);
        else
            NEXT_STEP_IMMEDIATE(sd, MYSQL_PING_FINAL, fd, events, arg);
    }
    else if (sd->ST == MYSQL_PING_CONT)
    {
        auto status = mysql_ping_cont(&sd->err, sd->mysql, mysql_status(events));
        if (status)
            async_next_event(MYSQL_PING_CONT, status, arg);
        else
            NEXT_STEP_IMMEDIATE(sd, MYSQL_PING_FINAL, fd, events, arg);
    }
    else if (sd->ST == MYSQL_PING_FINAL)
    {
        if (sd->err)
            NEXT_STEP_IMMEDIATE(sd, MYSQL_REAL_CONNECT_START, fd, events, arg);
        else
            NEXT_STEP_IMMEDIATE(sd, MYSQL_QUERY_ONCE_END, fd, events, arg);
    }
    /* Close 阶段 */
    else if (sd->ST == MYSQL_CLOSE_START)
    {
        auto status = mysql_close_start(sd->mysql);
        if (status)
            async_next_event(MYSQL_CLOSE_CONT, status, arg);
        else
            NEXT_STEP_IMMEDIATE(sd, MYSQL_CLOSE_FREE, fd, events, arg);
    }
    else if (sd->ST == MYSQL_CLOSE_CONT)
    {
        auto status = mysql_close_cont(sd->mysql, mysql_status(events));
        if (status)
            async_next_event(MYSQL_CLOSE_CONT, status, arg);
        else
            NEXT_STEP_IMMEDIATE(sd, MYSQL_CLOSE_FREE, fd, events, arg);
    }
    else if (sd->ST == MYSQL_CLOSE_FREE)
    {
        /* We are done! */
        event_free(sd->ev_mysql);
        //delete sd;
    }
}

#ifdef __DEBUG__
static void sigal_callback_(evutil_socket_t fd, short events, void *arg)
{
    task_ctx_t *m_ptr = (task_ctx_t *)arg;

    event_free(m_ptr->sigint_event);
    event_free(m_ptr->sighup_event);
    event_free(m_ptr->sigterm_event);
    event_free(m_ptr->worker_event);

    /* 关闭mysql */
    m_ptr->asyncmysql_ptr_->free_async_mysql(m_ptr);
}

static void worker_callback_(evutil_socket_t fd, short events, void *arg)
{
    task_ctx_t *m_ptr = (task_ctx_t *)arg;
    auto as = m_ptr->asyncmysql_ptr_;

    /*测试用，工作线程定时插入update语句*/
    std::string query = "update testdb.test_table set tx_result=1 where tx_id=1;";
    as->async_excute_query(query, 1);
}

static void after_select_callback_(char **row, int num, void *arg)
{
    task_ctx_t *m_ptr = (task_ctx_t *)arg;
    
    for (auto i = 0; i < num; i++)
        printf("%s%s", (i ? "\t" : ""), (row[i] ? row[i] : "(null)"));
    printf("\n");
}

static void after_update_callback_(int64_t tx_id, void *arg)
{
    task_ctx_t *m_ptr = (task_ctx_t *)arg;
    
    printf("after_update_callback_ tx_id: %d\n", tx_id);
}

int main()
{
    mysql_library_init(0, NULL, NULL);

    auto _evbase = event_base_new();

    std::shared_ptr<task_ctx_t> m_ptr = std::make_shared<task_ctx_t>();

    /* 模拟工作事务，不断生成update语句 */
    struct timeval work_sec = { 1, 0 };
    m_ptr->worker_event = event_new(_evbase, -1, EV_TIMEOUT | EV_PERSIST, worker_callback_, m_ptr.get());
    event_add(m_ptr->worker_event, &work_sec);

    m_ptr->asyncmysql_ptr_ = std::make_shared<CAsyncMysql>(dbhost, dbuser, dbpass, dbport);
    m_ptr->asyncmysql_ptr_->registerHandler(after_select_callback_, after_update_callback_, after_update_callback_);
    std::string query = "select tx_id,tx_url from testdb.test_table where tx_result=0 limit 10;";
    m_ptr->asyncmysql_ptr_->init_async_mysql(_evbase, query, m_ptr.get());

    /* 信号处理事件 */
    m_ptr->sigint_event  = evsignal_new(_evbase, SIGINT,  sigal_callback_, m_ptr.get());
    m_ptr->sigterm_event = evsignal_new(_evbase, SIGTERM, sigal_callback_, m_ptr.get());
    m_ptr->sighup_event  = evsignal_new(_evbase, SIGHUP,  sigal_callback_, m_ptr.get());
    evsignal_add(m_ptr->sigint_event, NULL);
    evsignal_add(m_ptr->sigterm_event, NULL);
    evsignal_add(m_ptr->sighup_event, NULL);

    event_base_dispatch(_evbase);
    event_base_free(_evbase);

    mysql_library_end();
}
#endif

