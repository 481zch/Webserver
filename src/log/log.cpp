#include "log/log.h"


Log *Log::getInstance()
{
    static Log log;
    return &log;
}

Log::Log(int maxLines, std::string saveDir, std::string suffix, bool isClose): 
                                            MAX_LINES(maxLines),
                                            m_saveDir(saveDir),
                                            m_suffix(suffix),
                                            m_isClose(isClose)
{
    m_lineCount = 0;
    m_deque = std::make_unique<BlockQueue<std::string>>();
    m_writeThread = std::make_unique<std::thread>(&Log::FlushLogThread);
    changeFile();
}

Log::~Log()
{
    while (!m_deque->empty()) {
        m_deque->flush();
    }
    m_deque->close();

    if (m_writeThread->joinable()) {
        m_writeThread->join();
    }
    
    if (m_fp) {
        std::unique_lock<std::mutex> lock(m_mtx);
        fclose(m_fp);
    }
}

void Log::FlushLogThread()
{
    Log::getInstance()->asyncWrite();
}

void Log::asyncWrite()
{
    std::string str;
    while (m_deque->pop(str)) {
        std::unique_lock<std::mutex> lock(m_mtx);
        fputs(str.c_str(), m_fp);
        fflush(m_fp);  // 强制刷新文件缓冲区进行写入，也不能太频繁的去刷新
    }
}

bool Log::isClosed()
{
    std::unique_lock<std::mutex> lock(m_mtx);
    return m_isClose;
}

// 需要对日志写入记录,不能够大于1024
// 没有更新时间，导致都在同一秒了
void Log::write(int level, const char *format, ...)
{
    Day temp = getToday();
    char message[1024];

    if ((temp != m_today) || (m_lineCount >= MAX_LINES))
        changeFile();

    va_list args;
    va_start(args, format);
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        ++m_lineCount;
        
        m_buff.Append(temp.transToString());
        appendLogLevelTitle(level);
        vsnprintf(message, sizeof(message), format, args);
        m_buff.Append(message);
        m_buff.Append("\n");

        m_deque->push_back(m_buff.getReadableBytes());
    }
    va_end(args);
}

void Log::appendLogLevelTitle(int level)
{
    switch(level) {
    case 0:
        m_buff.Append("[debug]: ", 9);
        break;
    case 1:
        m_buff.Append("[info] : ", 9);
        break;
    case 2:
        m_buff.Append("[warn] : ", 9);
        break;
    case 3:
        m_buff.Append("[error]: ", 9);
        break;
    default:
        m_buff.Append("[info] : ", 9);
        break;
    }
}

std::string Log::produceFileName()
{
    m_today = getToday();

    std::ostringstream oss;
    oss << m_saveDir << "/" 
        << (m_today.year + 1900) << "_"
        << std::setw(2) << std::setfill('0') << (m_today.month + 1) << "_"
        << std::setw(2) << std::setfill('0') << (m_today.day + 1) << "_"
        << std::setw(2) << std::setfill('0') << m_today.seconds
        << m_suffix;

    return oss.str();
}

void Log::changeFile()
{
    std::unique_lock<std::mutex> lock(m_mtx);
    m_logName = produceFileName();
    m_fp = fopen(m_logName.c_str(), "a");
    assert(m_fp != nullptr);
    m_lineCount = 0;
}

Day Log::getToday()
{
    time_t timer = time(nullptr);
    tm t = *localtime(&timer);

    Day res;
    res.year = t.tm_year;
    res.month = t.tm_mon;
    res.day = t.tm_mday;
    res.seconds = t.tm_sec;
    res.hour = t.tm_hour;
    res.minute = t.tm_min;

    return res;
}