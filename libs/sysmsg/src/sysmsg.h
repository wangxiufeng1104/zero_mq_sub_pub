#ifndef __SYS_MSG_H
#define __SYS_MSG_H
#ifdef __cplusplus
extern "C"
{ // only need to export C interface if
  // used by C++ source code
#endif
#include <stdint.h>
#include <pthread.h>
#include "czmq.h"
#define IPC_SUB "ipc://sysmsg_sub.ipc"
#define IPC_PUB "ipc://sysmsg_pub.ipc"
#define TCP_SUB "tcp://127.0.0.1:5555"
#define TCP_PUB "tcp://127.0.0.1:5556"
#define TCP_ROUTE "tcp://*:5557"
#define TCP_DEALER "tcp://*:5558"
#define TCP_DEALER_CON "tcp://127.0.0.1:5558"
#define TCP_ROUTE_CON "tcp://127.0.0.1:5557"
typedef void (*callback)(uint32_t topic_id, char *identify, uint8_t *data, int data_len);
typedef struct sysmsg_handle sysmsg_handle_t;
#pragma pack(1)
typedef struct
{
    uint16_t dlc;    // data length in byte
    uint8_t channel; // bit0~5: 1 based channel number; bit 6~7: protocol 00b - CAN, 10b - LIN, 01b - FlexRay, 11b - system
    uint8_t reserved;
    uint64_t timestamp; // timestamp in us
    uint32_t id;
    uint8_t data[0]; // variable length
} sysmsg_bmr_dat_msg_t;
#pragma pack()

#define SYSMSG_LOG_DEBUG (0x0001)
#define SYSMSG_LOG_INFO (0x0002)
#define SYSMSG_LOG_WARN (0x0003)
#define SYSMSG_LOG_ERROR (0x0004)
#define SYSMSG_LOG_HEX (0x0005)

#define SYSMSG_MDF_REC_CMD_ID (0x0014)
#define SYSMSG_MDF_REC_STS_ID (0x0015)
#define SYSMSG_BMR_REC_CMD_ID (0x0016)
#define SYSMSG_BMR_REC_STS_ID (0x0017)

#define SYSMSG_ILLEGAL_ID (0xFFFFFFFF)
/**
 * @brief connect pub and sub socket to sysmsg_proxy
 *
 * @param node_name :process name
 * @param sub_topic:array of topics to care about
 * @param sub_topic_num :number of topics of interest
 * @param cb: Callback function that needs to process data
 * @return sysmsg_handle_t *
 *     @retval NULL failed
 *     @retval not null init success
 */
sysmsg_handle_t *sysmsg_init(const char *node_name, uint32_t *sub_topic, uint32_t sub_topic_num, callback cb);
/**
 * @brief deinit 
 *
 * @param sysmsg_handle_t * 
 */
void sysmsg_deinit(sysmsg_handle_t *h);
/**
 * @brief Send a message of a specified topic
 *
 * @param sysmsg_handle_t :sysmsg_handle_t prt
 * @param topic:array of topics to care about
 * @param data :number of topics of interest
 * @param len: Callback function that needs to process data
 * @return sysmsg_handle_t *
 *     @retval -1 send failed, get result by sysmsg_geterr
 *     @retval success send data len
 */
int sysmsg_send(sysmsg_handle_t *h, uint32_t topic, void *data, uint8_t len);
/**
 * @brief get error result
 * @return error result
 */
const char *sysmsg_geterr(void);
/**
 * @brief Don't use this interface directly, you will lose function names and line numbers
 */
int32_t sysmsg_hex(sysmsg_handle_t *h, uint32_t level, const char *func, int32_t lineNum, uint8_t *data, int len);
/**
 * @brief Don't use this interface directly, you will lose function names and line numbers
 */
int32_t sysmsg_console(sysmsg_handle_t *h, uint32_t level, const char *func, int32_t lineNum, const char *format, ...);
/**
 * @brief sysmsg debug leval log
 */
#define LOG_DBG(h, fmt, args...) sysmsg_console(h, SYSMSG_LOG_DEBUG, __func__, __LINE__, fmt, ##args)
/**
 * @brief sysmsg info leval log
 */
#ifdef LOG_INFO
#undef LOG_INFO
#define LOG_INFO(h, fmt, args...) sysmsg_console(h, SYSMSG_LOG_INFO, __func__, __LINE__, fmt, ##args)
#endif
/**
 * @brief sysmsg warn leval log
 */
#define LOG_WARN(h, fmt, args...) sysmsg_console(h, SYSMSG_LOG_WARN, __func__, __LINE__, fmt, ##args)
/**
 * @brief sysmsg error leval log
 */
#ifdef LOG_ERR
#undef LOG_ERR
#define LOG_ERR(h, fmt, args...) sysmsg_console(h, SYSMSG_LOG_ERROR, __func__, __LINE__, fmt, ##args)
#endif
/**
 * @brief sysmsg hex log
 */
#define LOG_HEX(h, data, len) sysmsg_hex(h, SYSMSG_LOG_DEBUG, __func__, __LINE__, data, len)
#ifdef __cplusplus
}
#endif
#endif