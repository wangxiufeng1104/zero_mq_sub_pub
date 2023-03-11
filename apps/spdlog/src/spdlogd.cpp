// 输出格式请参考https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
#include <cstdio>
#include <chrono>
#include <iostream>
#define SPDLOG_NAME "spdlog"
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE // 必须定义这个宏,才能输出文件名和行号
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/fmt/bin_to_hex.h"
#include <signal.h>
#include <libgen.h>
#include "sysmsg.h"
#include "sys_until.h"

using namespace std;
uint32_t exit_flag = 1;
std::shared_ptr<spdlog::logger> rotating_logger;
void data_handle_cb(uint32_t topic, char *identify, uint8_t *data, int data_len)
{

    switch (topic)
    {
    case SYSMSG_LOG_DEBUG:
    {
        std::string str = reinterpret_cast<char *>(data);
        rotating_logger->debug(str);
    }
    break;
    case SYSMSG_LOG_INFO:
    {
        std::string str = reinterpret_cast<char *>(data);
        rotating_logger->info(str);
    }
    break;
    case SYSMSG_LOG_WARN:
    {
        std::string str = reinterpret_cast<char *>(data);
        rotating_logger->warn(str);
    }
    break;
    case SYSMSG_LOG_ERROR:
    {
        std::string str = reinterpret_cast<char *>(data);
        rotating_logger->error(str);
    }
    break;
    case SYSMSG_LOG_HEX:
    {
        std::vector<uint8_t> buf(data, data + data_len);
        rotating_logger->info("{:a}", spdlog::to_hex(buf, 16));
    }
    break;
    default:
        break;
    }
}
void signal_hander(int sig)
{
    printf("signal %d\n", sig);
    exit_flag = 0;
}
sysmsg_handle_t *h;
int main(int argc, char *argv[])
{
    // Create a file rotating logger with 5mb size max and 3 rotated files

    int ret = 0;
    uint32_t topic[] = {SYSMSG_LOG_INFO, SYSMSG_LOG_DEBUG, SYSMSG_LOG_WARN, SYSMSG_LOG_ERROR, SYSMSG_LOG_HEX};
    becomeSingle("spdlogd");
    signal(SIGTERM, signal_hander);
    signal(SIGINT, signal_hander);
    auto max_size = 1024 * 1024 * 100;
    auto max_files = 5;
    rotating_logger = spdlog::rotating_logger_mt(SPDLOG_NAME, "./log.txt", max_size, max_files);
    spdlog::flush_every(std::chrono::seconds(1));
    spdlog::set_default_logger(rotating_logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("%v");

    h = sysmsg_init(basename(argv[0]), topic, sizeof(topic) / sizeof(topic[0]), data_handle_cb);
    assert(h != NULL);

    while (exit_flag)
    {
        sleep(1);
    }
    sysmsg_deinit(h);
}