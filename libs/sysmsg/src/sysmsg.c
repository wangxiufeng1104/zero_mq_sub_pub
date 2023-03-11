#include "zmq.h"
#include "sysmsg.h"
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <czmq.h>
#include <errno.h>
#include "hiredis/sds.h"
struct sysmsg_handle
{
    /* psb socket */
    void *pub;
    /* sub socket */
    void *sub;
    uint32_t *topic;
    int topic_num;
    /* socket context */
    void *context;
    /* sender identify*/
    char identify[30];
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    zlist_t *zlist;
    /* run status */
    uint32_t runstatus;
    /* user callback */
    callback cb;
};
typedef struct
{
    uint32_t topic;
    int len;
    uint8_t buf[0];
} tpc_msg_t;

int s_send(void *socket, char *string);
void *sub_task(void *argv);
void *pub_task(void *argv);
char *sysmsg_recv_data(void *socket, int *data_len);
int s_sendmore_uint32(void *socket, uint32_t *sub_num);
int s_sendmore(void *socket, char *string);
int s_send_data(void *socket, uint8_t *data, uint8_t len);
/* 初始化订阅与发布信息handle */
sysmsg_handle_t *sysmsg_init(const char *node_name, uint32_t *sub_topic, uint32_t sub_topic_num, callback cb)
{
    int ret = 0;
    int rc = 0;
    assert(node_name != NULL);
    sysmsg_handle_t *h = malloc(sizeof(sysmsg_handle_t));
    assert(h != NULL);
    h->cb = cb;
    h->runstatus = 1;
    if (node_name != NULL)
    {
        memcpy(h->identify, node_name, strlen(node_name) > sizeof(h->identify) ? sizeof(h->identify) : strlen(node_name));
    }
    pthread_mutex_init(&h->mutex, NULL);
    pthread_cond_init(&h->cond, NULL);
    h->zlist = zlist_new();
    pthread_attr_t att;
    pthread_attr_init(&att);
    pthread_attr_setdetachstate(&att, PTHREAD_CREATE_DETACHED);
    h->context = zmq_ctx_new();
    pthread_t tid_pub;
    pthread_create(&tid_pub, &att, pub_task, h);
    if ((sub_topic != NULL) && (sub_topic_num > 0))
    {
        h->topic_num = sub_topic_num;
        h->topic = (uint32_t *)malloc(sub_topic_num * sizeof(uint32_t));
        assert(h->topic != NULL);
        memcpy(h->topic, sub_topic, sub_topic_num * sizeof(uint32_t));
        pthread_t tid_sub;
        pthread_create(&tid_sub, &att, sub_task, h);
    }
    return h;
}
const char *sysmsg_geterr(void)
{
    return zmq_strerror(zmq_errno());
}
void sysmsg_deinit(sysmsg_handle_t *h)
{
    if (h)
    {
        if (h->sub)
        {
            zmq_close(h->sub);
            h->sub = NULL;
        }
        if (h->pub)
        {
            zmq_close(h->pub);
            h->pub = NULL;
        }
        if (h->context)
        {
            zmq_ctx_destroy(h->context);
            h->context = NULL;
        }
        h->cb = NULL;
        h->runstatus = 0;

        zlist_destroy(&h->zlist);
        free(h);
    }
}
char *s_recv(void *socket)
{
    // 创建zmq_msg_t对象接收数据
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    int size = zmq_msg_recv(&msg, socket, 0);
    if (size == -1)
    {
        return NULL;
    }

    // 将zmq_msg_t对象中的数据保存到字符串中
    char *string = (char *)malloc(size + 1);
    memcpy(string, zmq_msg_data(&msg), size);

    zmq_msg_close(&msg);
    string[size] = 0;

    return string;
}
uint32_t s_recv_topie(void *socket)
{
    // 创建zmq_msg_t对象接收数据
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    int size = zmq_msg_recv(&msg, socket, 0);
    if (size == -1)
    {
        return SYSMSG_ILLEGAL_ID;
    }
    uint32_t topic;
    memcpy(&topic, zmq_msg_data(&msg), size);
    zmq_msg_close(&msg);
    return topic;
}
void *pub_task(void *argv)
{
    int rc = 0;
    tpc_msg_t *item = NULL;
    sysmsg_handle_t *h = (sysmsg_handle_t *)argv;
    h->pub = zmq_socket(h->context, ZMQ_PUB);
    assert(h->pub);

    rc = zmq_connect(h->pub, TCP_SUB);
    if (rc != 0)
    {
        zsys_error("pub bind failed:%s\n", zmq_strerror(zmq_errno()));
        exit(1);
    }
    while (h->runstatus)
    {
        pthread_mutex_lock(&h->mutex);
        while ((tpc_msg_t *)zlist_first(h->zlist) == NULL)
        {
            pthread_cond_wait(&h->cond, &h->mutex);
        }
        item = (tpc_msg_t *)zlist_pop(h->zlist);
        pthread_mutex_unlock(&h->mutex);
        rc = s_sendmore_uint32(h->pub, &item->topic);
        if (rc == -1 && (zmq_errno() == ENOTSOCK || zmq_errno() == ETERM))
        {
            zsys_error("%s", zmq_strerror(zmq_errno()));
            break;
        }
        if (item->len != 0)
        {
            rc = s_sendmore(h->pub, h->identify);
            if (rc == -1 && (zmq_errno() == ENOTSOCK || zmq_errno() == ETERM))
            {
                zsys_error("%s", zmq_strerror(zmq_errno()));
                break;
            }
            rc = s_send_data(h->pub, (unsigned char *)item->buf, item->len);
            if (rc == -1 && (zmq_errno() == ENOTSOCK || zmq_errno() == ETERM))
            {
                zsys_error("%s", zmq_strerror(zmq_errno()));
                break;
            }
        }
        else
        {
            rc = s_send(h->pub, h->identify);
            if (rc == -1 && (zmq_errno() == ENOTSOCK || zmq_errno() == ETERM))
            {
                zsys_error("%s", zmq_strerror(zmq_errno()));
                break;
            }
        }
        free(item);
    }
}

void *sub_task(void *argv)
{
    sysmsg_handle_t *h = (sysmsg_handle_t *)argv;
    int more;
    int data_len;
    uint8_t *data = NULL;
    uint8_t *identify = NULL;
    int rc = 0;
    size_t more_size;
    h->sub = zmq_socket(h->context, ZMQ_SUB);
    assert(h->sub);

    rc = zmq_connect(h->sub, TCP_PUB);
    if (!rc)
    {
        for (int i = 0; i < h->topic_num; i++)
        {
            rc = zmq_setsockopt(h->sub, ZMQ_SUBSCRIBE, &h->topic[i], sizeof(uint32_t));
        }
    }
    else
    {
        zsys_error("%s", zmq_strerror(zmq_errno()));
        exit(1);
    }

    while (h->runstatus)
    {
        uint32_t topic = s_recv_topie(h->sub);
        if (topic == SYSMSG_ILLEGAL_ID)
        {
            if (zmq_errno() == ENOTSOCK || zmq_errno() == ETERM)
            {
                break;
            }
            else
            {
                zsys_error("%d,%s:%d\n", __LINE__, zmq_strerror(zmq_errno()), zmq_errno());
                continue;
            }
        }

        identify = s_recv(h->sub);
        if (identify == NULL)
        {
            zsys_error("%d,%s:%d\n", __LINE__, zmq_strerror(zmq_errno()), zmq_errno());
            continue;
        }
        more_size = sizeof(more);
        zmq_getsockopt(h->sub, ZMQ_RCVMORE, &more, &more_size);
        if (!more)
        {
            if (h->cb != NULL)
                h->cb(topic, identify, NULL, 0);
            free(identify);
            continue;
        }
        do
        {
            data = sysmsg_recv_data(h->sub, &data_len);
            if (data == NULL)
            {
                zsys_error("%d,%s:%d\n", __LINE__, zmq_strerror(zmq_errno()), zmq_errno());
                break;
            }
            if (h->cb != NULL)
                h->cb(topic, identify, data, data_len);
            free(data);
            more_size = sizeof(more);
            zmq_getsockopt(h->sub, ZMQ_RCVMORE, &more, &more_size);
        } while (more);
        free(identify);
    }
}
char *sysmsg_recv_data(void *socket, int *data_len)
{
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    int size = zmq_msg_recv(&msg, socket, 0);
    if (size == -1)
    {
        return NULL;
    }
    char *data = (char *)malloc(size + 1);
    memcpy(data, zmq_msg_data(&msg), size);
    zmq_msg_close(&msg);
    data[size] = 0;
    *data_len = size;
    return data;
}
int s_sendmore(void *socket, char *string)
{
    int rc;
    uint8_t filter[11];
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, strlen(string));
    memcpy(zmq_msg_data(&msg), string, strlen(string));
    rc = zmq_msg_send(&msg, socket, ZMQ_SNDMORE);
    zmq_msg_close(&msg);
    return (rc);
}
int s_send(void *socket, char *string)
{
    int rc;
    uint8_t filter[11];
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, strlen(string));
    memcpy(zmq_msg_data(&msg), string, strlen(string));
    rc = zmq_msg_send(&msg, socket, 0);
    zmq_msg_close(&msg);
    return (rc);
}
int sysmsg_send(sysmsg_handle_t *h, uint32_t topic, void *data, uint8_t len)
{
    int rc = 0;

    tpc_msg_t *msg = malloc(sizeof(tpc_msg_t) + len);
    if (msg != NULL)
    {
        msg->topic = topic;
        msg->len = len;
        if (data != NULL && len != 0)
        {
            memcpy(msg->buf, data, len);
        }
        else
        {
            msg->len = 0;
        }
        pthread_mutex_lock(&h->mutex);
        zlist_append(h->zlist, msg);
        pthread_mutex_unlock(&h->mutex);
        pthread_cond_signal(&h->cond);
    }
    else
    {
        rc = -1;
    }
    return rc;
}
int s_sendmore_uint32(void *socket, uint32_t *sub_num)
{
    int rc;

    zmq_msg_t msg;
    zmq_msg_init_size(&msg, sizeof(uint32_t));
    memcpy(zmq_msg_data(&msg), sub_num, sizeof(uint32_t));
    rc = zmq_msg_send(&msg, socket, ZMQ_SNDMORE);
    zmq_msg_close(&msg);
    return (rc);
}
int s_send_data(void *socket, uint8_t *data, uint8_t len)
{
    // 初始化一个zmq_msg_t对象, 分配的大小为string的大小
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, len);
    memcpy(zmq_msg_data(&msg), data, len);

    // 发送数据
    int rc = zmq_msg_send(&msg, socket, 0);

    // 关闭zmq_msg_t对象
    zmq_msg_close(&msg);

    return rc;
}

int32_t sysmsg_hex(sysmsg_handle_t *h, uint32_t level, const char *func, int32_t lineNum, uint8_t *data, int len)
{
    int rc = 0;
    int index = 0;

    if (NULL == data || NULL == h || 0 >= len)
    {
        return -1;
    }
    tpc_msg_t *msg = malloc(sizeof(tpc_msg_t) + len);
    if (msg == NULL)
    {
        return -1;
    }
    sysmsg_console(h, level, func, lineNum, "HEX DATA");
    msg->topic = SYSMSG_LOG_HEX;
    msg->len = len;
    memcpy(msg->buf, data, len);
    for (; index < len; index++)
    {
        fprintf(stdout, "%02x ", *((uint8_t *)data + index) & 0xFF);
        if ((index + 1) % 16 == 0)
            fprintf(stdout, "\n");
    }
    fprintf(stdout, "\n");
    fflush(stdout);
    pthread_mutex_lock(&h->mutex);
    zlist_append(h->zlist, msg);
    pthread_mutex_unlock(&h->mutex);
    pthread_cond_signal(&h->cond);
    return 0;
}
int32_t sysmsg_console(sysmsg_handle_t *h, uint32_t level, const char *func, int32_t lineNum, const char *format, ...)
{
    va_list ap;
    int32_t ret = 0;
    int32_t len = 0;
    int rc = 0;
    struct timeval tv;
    struct tm *ptm = NULL;

    if (NULL == format || NULL == h)
    {
        return -1;
    }

    hisds s = hi_sdsempty();
    gettimeofday(&tv, NULL);
    ptm = localtime(&tv.tv_sec);
    s = hi_sdscatprintf(s, "[%04u-%02u-%02u %02u:%02u:%02u.%06lu]",
                  (ptm->tm_year + 1900), ptm->tm_mon + 1, ptm->tm_mday,
                  ptm->tm_hour, ptm->tm_min, ptm->tm_sec, tv.tv_usec % 1000000);
    
    s = hi_sdscatfmt(s, "[%s]", h->identify);
    switch (level)
    {
    case SYSMSG_LOG_DEBUG:
        s = hi_sdscatfmt(s, "[%s]", "DEBUG");
        break;
    case SYSMSG_LOG_INFO:
        s = hi_sdscatfmt(s, "[%s]", "INFO");
        break;
    case SYSMSG_LOG_WARN:
        s = hi_sdscatfmt(s, "[%s]", "WARN");
        break;
    case SYSMSG_LOG_ERROR:
        s = hi_sdscatfmt(s, "[%s]", "ERROR");
        break;
    default:
        s = hi_sdscatfmt(s, "[%s]", "DEBUG");
        break;
    }
    if (func)
    {
        s = hi_sdscatfmt(s, "[%s]", func);
    }
    if (lineNum > 0)
    {
        s = hi_sdscatfmt(s, "[%d]", lineNum);
    }
    va_start(ap, format);
    s = hi_sdscatvprintf(s, format, ap);
    va_end(ap);
    tpc_msg_t *msg = malloc(sizeof(tpc_msg_t) + hi_sdslen(s) + 1);
    if (msg == NULL)
    {
        hi_sdsfree(s);
        return -1;
    }
    memset(msg, 0, sizeof(tpc_msg_t) + hi_sdslen(s) + 1);
    msg->topic = level;
    msg->len = hi_sdslen(s);
    memcpy(msg->buf, s, msg->len);
    hi_sdsfree(s);
    fprintf(stdout, "%s\n", msg->buf);
    fflush(stdout);
    pthread_mutex_lock(&h->mutex);
    zlist_append(h->zlist, msg);
    pthread_mutex_unlock(&h->mutex);
    pthread_cond_signal(&h->cond);
    return 0;
}