#include "sysmsg.h"
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <libgen.h>
#include <assert.h>
#include <string.h>
#include <time.h>


uint32_t exit_flag = 1;
sysmsg_handle_t *h;
void signal_hander(int sig)
{
    printf("signal %d\n", sig);
    exit_flag = 0;
}
void data_handle_cb(uint32_t topic, char *identify, uint8_t *data, int data_len)
{
    printf("topic:%x\n", topic);
    sysmsg_bmr_dat_msg_t *msg = (sysmsg_bmr_dat_msg_t *)data;
    printf("msg->channel:%d\n", msg->channel);
    printf("msg->dlc:%d\n", msg->dlc);
    printf("msg->id:%x\n", msg->id);
    printf("msg->timestamp:%lu\n", msg->timestamp);
    LOG_DBG(h, "topic,%d", topic);
    LOG_INFO(h, "msg->channel:%d", msg->channel);
    LOG_DBG(h, "msg->dlc:%d", msg->dlc);
    LOG_WARN(h, "msg->id:%x", msg->id);
    LOG_WARN(h, "msg->timestamp:%lu", msg->timestamp);
    LOG_HEX(h, msg->data, msg->dlc);
}
uint8_t send_buf[100];
int main(int argc, char **argv)
{
    int ret = 0;
    int rc = 0;
    int channel = 0;
    uint8_t data[100] = {0};
    for (int i = 0; i < 100; i++)
    {
        data[i] = i;
    }
    
    uint32_t topic[] = {SYSMSG_MDF_REC_STS_ID};
    signal(SIGTERM, signal_hander); // 设置SIGTERM信号处理函数p
    signal(SIGINT, signal_hander);  // 设置SIGINT信号处理函数

    h = sysmsg_init(basename(argv[0]), topic, sizeof(topic) / sizeof(topic[0]), data_handle_cb);
    assert(h != NULL);
    sysmsg_bmr_dat_msg_t *msg = (sysmsg_bmr_dat_msg_t *)send_buf;
    while (exit_flag)
    {

        msg->channel = channel++;
        if(channel > 20)
            channel = 0;
        msg->dlc = 8;
        msg->timestamp = time(NULL);
        msg->id = 0x100;
        memcpy(msg->data, data, msg->dlc);
        rc = sysmsg_send(h, SYSMSG_MDF_REC_CMD_ID, msg, sizeof(sysmsg_bmr_dat_msg_t) + msg->dlc);
        //sleep(1);
        /* test data is NULL*/
        rc = sysmsg_send(h, SYSMSG_BMR_REC_STS_ID, NULL, sizeof(sysmsg_bmr_dat_msg_t) + msg->dlc);
        /* test len is 0*/
        rc = sysmsg_send(h, SYSMSG_BMR_REC_STS_ID, msg, 0);
        /* test data is NULL and len is 0*/
        rc = sysmsg_send(h, SYSMSG_BMR_REC_STS_ID, NULL, 0);
        
        // usleep(1000);
        //sleep(1);
    }
    printf("good bye\n");
    fflush(stdout);
    sysmsg_deinit(h);
    return 0;
}