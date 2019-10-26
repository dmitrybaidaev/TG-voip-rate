#include <assert.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <cstdarg>
#include "rate_log.h"

namespace tg_rate {

    void log(FILE* stream, const char *tag, const char* file, int line, const char* func, const char* fmt, ...) {
        if(stream == nullptr) {
            stream = stdout;
        }
        va_list args;
        va_start(args, fmt);
        char buffer[4096];
        size_t len = vsnprintf(buffer, sizeof(buffer), fmt, args);
        assert(len < sizeof(buffer) - 1);
        va_end(args);
#if !DEBUG
        fprintf(stream, "%s\n", buffer);
#else
        fprintf(stream, "%s(%4d): %s\n", func, line, buffer);
#endif
    }

}
