#pragma once

#include <stdio.h>

namespace tg_rate {
    void log(FILE* stream, const char *tag, const char* file, int line, const char* func, const char* fmt, ...);
}

#define RATE_LOG(stream, fmt, ...) { tg_rate::log(stream, "TG_RATE_LOG", __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__); }

#if !DEBUG
    #define RATE_LOGE(...) RATE_LOG(stderr, ##__VA_ARGS__)
    #define RATE_LOGI(...)
#else
    #define RATE_LOGE(fmt,...) RATE_LOG(stderr, fmt, ##__VA_ARGS__)
    #define RATE_LOGI(fmt,...) RATE_LOG(stdout, fmt, ##__VA_ARGS__)
#endif

