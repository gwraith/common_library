#include "cpluslog.h"

std::shared_ptr<CPlusLog> m_log = std::make_shared<CPlusLog>("normal-", "error-");

//************************************
// 函数名称:  deal_thread_norma_log_
// 函数参数:  void * arg
// 函数返回:  void *
// 函数说明:  normal日志处理线程
//************************************
void *deal_thread_norma_log_(void *arg)
{
    auto m_ptr = (CPlusLog *)arg;
    while (!m_ptr->b_endthread)
    {
        m_ptr->deal_norma_log_once();
        usleep(1000);
    }
    return arg;
}


//************************************
// 函数名称:  deal_thread_error_log_
// 函数参数:  void * arg
// 函数返回:  void *
// 函数说明:  error日志处理线程
//************************************
void *deal_thread_error_log_(void *arg)
{
    auto m_ptr = (CPlusLog *)arg;
    while (!m_ptr->b_endthread)
    {
        m_ptr->deal_error_log_once();
        usleep(1000);
    }
    return arg;
}

//************************************
// 函数名称:  init_cplus_log
// 函数返回:  bool
// 函数说明:  日志模块初始化
//************************************
bool CPlusLog::init_cplus_log()
{
    std::string szExe = string_format("/proc/%d/exe", getpid());
    char szName[1024] = { '\0' };
    if (readlink(szExe.c_str(), szName, sizeof(szName)) == -1) return false;
    m_chPath = std::string(szName, strlen(szName));
    m_chPath = m_chPath.substr(0, m_chPath.rfind('/') + 1) + std::string("log");

    if (access(m_chPath.c_str(), F_OK) == -1)
        mkdir(m_chPath.c_str(), S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

    if (!init_norma_log() || !init_error_log()) return false;

    delete_once_log(both_log);

    m_norma_thread = std::make_shared<std::thread>(deal_thread_norma_log_, this);
    m_error_thread = std::make_shared<std::thread>(deal_thread_error_log_, this);
    return true;
}

//************************************
// 函数名称:  deal_norma_log_once
// 函数返回:  void
// 函数说明:  normal日志处理
//************************************
void CPlusLog::deal_norma_log_once()
{
    if (m_norma_list.empty()) return;
    m_norma_mutex.lock();
    auto it = m_norma_list.begin();
    if (it == m_norma_list.end()) return;
    if (calibration_file_desc(nomal_log) && log_record(m_fpNorma, it->c_str()))
    {
        m_norma_list.pop_front();
    }
    m_norma_mutex.unlock();
    if (rotation_norma) delete_once_log(nomal_log);
}

//************************************
// 函数名称:  deal_error_log_once
// 函数返回:  void
// 函数说明:  错误日志处理
//************************************
void CPlusLog::deal_error_log_once()
{
    if (m_error_list.empty()) return;
    m_error_mutex.lock();
    auto it = m_error_list.begin();
    if (it == m_error_list.end()) return;
    if (calibration_file_desc(error_log) && log_record(m_fpError, it->c_str()))
    {
        m_error_list.pop_front();
    }
    m_error_mutex.unlock();
    if (rotation_error) delete_once_log(error_log);
}

//************************************
// 函数名称:  log_norma
// 函数参数:  std::string & msg
// 函数返回:  void
// 函数说明:  消息插入队列
//************************************
void CPlusLog::log_norma(std::string &msg)
{
    m_norma_mutex.lock();
    m_norma_list.emplace_back(std::move(msg));
    m_norma_mutex.unlock();
}

//************************************
// 函数名称:  log_error
// 函数参数:  std::string & msg
// 函数返回:  void
// 函数说明:  消息插入队列
//************************************
void CPlusLog::log_error(std::string &msg)
{
    m_error_mutex.lock();
    m_error_list.emplace_back(std::move(msg));
    m_error_mutex.unlock();
}

//************************************
// 函数名称:  init_norma_log
// 函数返回:  bool
// 函数说明:  normal日志初始化
//************************************
bool CPlusLog::init_norma_log()
{
    time_t now_time = system_clock::to_time_t(system_clock::now());
    struct tm tm_now;
    localtime_r(&now_time, &tm_now);
    m_chNormaName = string_format("%s/%s%04d-%02d-%02d.log", m_chPath.c_str(), norma_log_name.c_str(), tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);
    m_fpNorma = fopen(m_chNormaName.c_str(), "a+b");
    if (m_fpNorma == nullptr) return false;
    /* 不使用缓冲区 */
    setbuf(m_fpNorma, NULL);
    return true;
}

//************************************
// 函数名称:  init_error_log
// 函数返回:  bool
// 函数说明:  error日志初始化
//************************************
bool CPlusLog::init_error_log()
{
    time_t now_time = system_clock::to_time_t(system_clock::now());
    struct tm tm_now;
    localtime_r(&now_time, &tm_now);
    m_chErrorName = string_format("%s/%s%04d-%02d-%02d.log", m_chPath.c_str(), error_log_name.c_str(), tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);
    m_fpError = fopen(m_chErrorName.c_str(), "a+b");
    if (m_fpError == nullptr) return false;
    /* 不使用缓冲区 */
    setbuf(m_fpError, NULL);
    return true;
}

//************************************
// 函数名称:  calibration_file_desc
// 函数参数:  int type
// 函数返回:  bool
// 函数说明:  判断当前的文件描述符是否指向正确日期的文件
//************************************
bool CPlusLog::calibration_file_desc(int type)
{
    time_t now_time = system_clock::to_time_t(system_clock::now());
    struct tm tm_now;
    localtime_r(&now_time, &tm_now);

    if (type == nomal_log)
    {
        rotation_norma = false;
        std::string m_tmpName = string_format("%s/%s%04d-%02d-%02d.log", m_chPath.c_str(), norma_log_name.c_str(), tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);
        if (m_tmpName == m_chNormaName) return true;
        if (m_fpNorma) fclose(m_fpNorma);
        m_fpNorma = nullptr;
        rotation_norma = true;
        return init_norma_log();
    }
    else if(type == error_log)
    {
        rotation_error = false;
        std::string m_tmpName = string_format("%s/%s%04d-%02d-%02d.log", m_chPath.c_str(), error_log_name.c_str(), tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);
        if (m_tmpName == m_chErrorName) return true;
        if (m_fpError) fclose(m_fpError);
        m_fpError = nullptr;
        rotation_error = true;
        return init_error_log();
    }

    return false;
}

//************************************
// 函数名称:  log_record
// 函数参数:  FILE * fp
// 函数参数:  const char * buf
// 函数返回:  bool
// 函数说明:  写入日志文件
//************************************
bool CPlusLog::log_record(FILE *fp, const char *buf)
{
    if (fwrite(buf, sizeof(char), strlen(buf), fp) != strlen(buf))
        return false;
    return true;
}

//************************************
// 函数名称:  delete_once_log
// 函数参数:  int type
// 函数返回:  void
// 函数说明:  删除过期日志
//************************************
void CPlusLog::delete_once_log(int type)
{
    DIR *dir = opendir(m_chPath.c_str());
    if (dir == NULL)
    {
        LogErrorPara("open dir [%s] error [%s].", m_chPath.c_str(), strerror(errno));
        return;
    }

    time_t now_time = system_clock::to_time_t(system_clock::now()) - 7 * 24 * 3600;
    struct tm m_tNow;
    localtime_r(&now_time, &m_tNow);
    std::string szDate = string_format("%04d-%02d-%02d", m_tNow.tm_year + 1900, m_tNow.tm_mon + 1, m_tNow.tm_mday);

    struct dirent *dirp = NULL;
    while ((dirp = readdir(dir)) != NULL)
    {
        if (dirp->d_name == NULL || dirp->d_name[0] == '.' || strcmp(dirp->d_name, "..") == 0) continue;
        if ((type == nomal_log || type == both_log) && strncmp(dirp->d_name, norma_log_name.c_str(), norma_log_name.length()) == 0)
        {
            if (strncmp(dirp->d_name + norma_log_name.length(), szDate.c_str(), szDate.length()) < 0)
            {
                std::string tmp_name = string_format("%s/%s", m_chPath.c_str(), dirp->d_name);
                if (0 != unlink(tmp_name.c_str()))
                    LogErrorPara("delete normal log [%s] fail, err[%s].", tmp_name.c_str(), strerror(errno));
                else
                    LogDefaultPara("delete normal log [%s] successful.", tmp_name.c_str());
            }
        }
        if ((type == error_log || type == both_log) && strncmp(dirp->d_name, error_log_name.c_str(), error_log_name.length()) == 0)
        {
            if (strncmp(dirp->d_name + error_log_name.length(), szDate.c_str(), szDate.length()) < 0)
            {
                std::string tmp_name = string_format("%s/%s", m_chPath.c_str(), dirp->d_name);
                if (0 != unlink(tmp_name.c_str()))
                    LogErrorPara("delete error log [%s] fail, err[%s].", tmp_name.c_str(), strerror(errno));
                else
                    LogDefaultPara("delete error log [%s] successful.", tmp_name.c_str());
            }
        }
    }
    closedir(dir);
}

#if 0
int main()
{
    m_log->init_cplus_log();
    LogDefaultPara("name: %s age: %d", "gaoyaming", 20);
    LogErrorPara("name: %s age: %d", "jack", 30);
    printf("11111\n");
    printf("22222\n");
    return 0;
}
#endif
