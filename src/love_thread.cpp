#include "love_thread.h"

std::map<std::string, LoveChannel*> LoveThreadManager::s_channels;

LoveChannel *LoveThreadManager::getChannel(const std::string &name) {
    if (s_channels.find(name) == s_channels.end()) {
        s_channels[name] = new LoveChannel(name);
    }
    return s_channels[name];
}

void LoveThreadManager::cleanup() {
    for (std::map<std::string, LoveChannel*>::iterator it = s_channels.begin(); it != s_channels.end(); ++it) {
        delete it->second;
    }
    s_channels.clear();
}

LoveThread::LoveThread(const std::string &code_or_file)
    : m_target(code_or_file), m_running(false) {
}

LoveThread::~LoveThread() {
}

void LoveThread::start() {
    m_running = true;
}
