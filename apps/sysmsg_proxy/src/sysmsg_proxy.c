#include <stdio.h>
#include <zmq.h>
#include "sysmsg.h"
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include "sys_until.h"
int main()
{
    int io_threads = 4;
    void *context = NULL;
    void *frontend = NULL;
    void *backend = NULL;
    becomeSingle("sysmsg_proxy");
    context = zmq_ctx_new();
    zmq_ctx_set(context, ZMQ_IO_THREADS, io_threads);
    assert(zmq_ctx_get(context, ZMQ_IO_THREADS) == io_threads);
    frontend = zmq_socket(context, ZMQ_XSUB);
    zmq_bind(frontend, TCP_SUB);
    backend = zmq_socket(context, ZMQ_XPUB);
    zmq_bind(backend, TCP_PUB);
    zmq_proxy(frontend, backend, NULL);
    zmq_close(frontend);
    zmq_close(backend);
    zmq_ctx_destroy(context);
    return 0;
}
