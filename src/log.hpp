#pragma once
#include <ctime>


#define INFO 0
#define DEBUG 1
#define ERR 2
#define DEFAULT_LOG_LEVEL DEBUG

#define LOG(level, format, ...) do{\
    if(level < DEFAULT_LOG_LEVEL) break;\
    time_t t = time(nullptr);\
    struct tm* ltm = localtime(&t);\
    char tmp[32] = {0};\
    strftime(tmp, 31, "%H:%M:%S", ltm);\
    fprintf(stdout, "%s %s:%d " format "\n", tmp, __FILE__, __LINE__, ##__VA_ARGS__);\
}while(0)
#define INFO_LOG(format, ...) LOG(INFO, format, ##__VA_ARGS__)
#define DEBUG_LOG(format, ...) LOG(DEBUG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)
