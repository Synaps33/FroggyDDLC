#ifndef LOVE_THREAD_H
#define LOVE_THREAD_H

#include <string>
#include <vector>
#include <map>
#include "love_common.h"

struct ChannelMsg {
    std::string str_val;
    double num_val;
    bool is_num;
    bool is_table_stub;
};

class LoveChannel {
public:
    LoveChannel(const std::string &name) : m_name(name) {}

    void push(const ChannelMsg &msg) {
        m_queue.push_back(msg);
    }

    bool pop(ChannelMsg *out_msg) {
        if (m_queue.empty()) return false;
        *out_msg = m_queue.front();
        m_queue.erase(m_queue.begin());
        return true;
    }

    size_t getCount() const { return m_queue.size(); }
    void clear() { m_queue.clear(); }

private:
    std::string m_name;
    std::vector<ChannelMsg> m_queue;
};

class LoveThread {
public:
    LoveThread(const std::string &code_or_file);
    ~LoveThread();

    void start();
    bool isRunning() const { return m_running; }

private:
    std::string m_target;
    bool m_running;
};

class LoveThreadManager {
public:
    static LoveChannel *getChannel(const std::string &name);
    static void cleanup();

private:
    static std::map<std::string, LoveChannel*> s_channels;
};

#endif // LOVE_THREAD_H
