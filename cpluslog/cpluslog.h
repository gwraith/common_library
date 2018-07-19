#ifndef __CPLUS_LOG_MONITOR_H__
#define __CPLUS_LOG_MONITOR_H__
/************************************************************************/
// 日志模块
// [7/19/2018 gaoyaming]
/************************************************************************/

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <dirent.h>

#include <list>
#include <mutex>
#include <chrono>
#include <string>
#include <memory>
#include <thread>

#include <sys/stat.h>
#include <sys/types.h>

using namespace std::chrono;

template <typename T>
struct delete_ptr
{
    void operator()(T* p) { delete[] p; }
};

int vscprintf(const char *format, ...)
{
    va_list marker;
    va_start(marker, format);
    int num = vsnprintf(0, 0, format, marker);
    va_end(marker);
    return num;
}

std::string std_string_format(int num_of_chars, const char * format, ...)
{
    std::shared_ptr<char> tmp_buffer(new char[num_of_chars], delete_ptr<char>());
    va_list marker;
    va_start(marker, format);
    int ret = vsnprintf(tmp_buffer.get(), num_of_chars, format, marker);
    va_end(marker);
    return std::string(tmp_buffer.get(), ret);
}

template <typename... Args>
std::string string_format(Args&&... args)
{
    return std_string_format(vscprintf(std::forward<Args>(args)...) + 1, std::forward<Args>(args)...);
}

#define nomal_log 1
#define error_log 2
#define both_log 3

class CPlusLog
{
public:
    CPlusLog(std::string szNormal, std::string szError) : norma_log_name(std::move(szNormal)), error_log_name(std::move(szError)) {
        b_endthread = false;
        rotation_norma = false;
        rotation_error = false;
        m_fpNorma = nullptr;
        m_fpError = nullptr;
    };
    ~CPlusLog() {
        while (!m_norma_list.empty() || !m_error_list.empty())
            sleep(1);
        
        /* 线程退出 */
        b_endthread = true;
        if (m_norma_thread->joinable()) m_norma_thread->join();
        if (m_error_thread->joinable()) m_error_thread->join();

        /* 关闭描述符 */
        if (m_fpNorma) fclose(m_fpNorma);
        if (m_fpError) fclose(m_fpError);
    };

public:
    bool init_cplus_log();

    void deal_norma_log_once();
    void deal_error_log_once();

    void log_norma(std::string &msg);
    void log_error(std::string &msg);

public:
    bool b_endthread;
    std::shared_ptr<std::thread> m_norma_thread;
    std::shared_ptr<std::thread> m_error_thread;

private:
    bool rotation_norma;
    bool rotation_error;

    bool init_norma_log();
    bool init_error_log();

    bool calibration_file_desc(int type);
    bool log_record(FILE *fp, const char *buf);

    void delete_once_log(int type);
private:
    std::string m_chPath;
    
    std::string norma_log_name, m_chNormaName;
    std::string error_log_name, m_chErrorName;

    FILE *m_fpNorma;
    FILE *m_fpError;

    std::mutex m_norma_mutex;
    std::mutex m_error_mutex;

    std::list<std::string> m_norma_list;
    std::list<std::string> m_error_list;
};

extern std::shared_ptr<CPlusLog> m_log;

template <typename... Args>
void LogDefaultPara(Args&&... args)
{
    std::string chMsg = string_format(std::forward<Args>(args)...);
    time_t timep = system_clock::to_time_t(system_clock::now());
    struct tm m_tnow = { 0 };
    localtime_r(&timep, &m_tnow);
    std::string tmpmsg = string_format("%04d-%02d-%02d %02d:%02d:%02d %s\r\n", m_tnow.tm_year + 1900, m_tnow.tm_mon + 1, m_tnow.tm_mday, m_tnow.tm_hour, m_tnow.tm_min, m_tnow.tm_sec, chMsg.c_str());
    printf("%s", tmpmsg.c_str());
    m_log->log_norma(tmpmsg);
}

template <typename... Args>
void LogErrorPara(Args&&... args)
{
    std::string chMsg = string_format(std::forward<Args>(args)...);
    time_t timep = system_clock::to_time_t(system_clock::now());
    struct tm m_tnow = { 0 };
    localtime_r(&timep, &m_tnow);
    std::string tmpmsg = string_format("%04d-%02d-%02d %02d:%02d:%02d %s\r\n", m_tnow.tm_year + 1900, m_tnow.tm_mon + 1, m_tnow.tm_mday, m_tnow.tm_hour, m_tnow.tm_min, m_tnow.tm_sec, chMsg.c_str());
    printf("%s", tmpmsg.c_str());
    m_log->log_error(tmpmsg);
}
#endif
