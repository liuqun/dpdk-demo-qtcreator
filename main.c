// Ubuntu需要安装的开发工具:
// sudo apt-get install dpdk-dev
// sudo apt-get install pkg-config

#include <stdio.h>
#include <dpdk/rte_ring.h>
#include <dpdk/rte_lcore.h>


#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
//#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
struct item_ {
    void * data;
};

struct my_thrd_args_ {
    struct rte_ring *r;
};
//typedef struct myarg_ myarg;

static int lcore_hello(void *arg_)
{

    unsigned lcore_id;
    lcore_id = rte_lcore_id();
    printf("hello from core %u\n", lcore_id);

    struct rte_ring *r = ((struct my_thrd_args_ *)arg_)->r;

#define N 8
    void * const obj_table[N] = {
        "1",
        "2",
        "3",
        "4",
        "5",
        "6",
        "7",
        "8",
    };
    unsigned int n_empty_cells = 0;
    unsigned int n_enqueue_ok = 0;
    unsigned int n_need_enqueue = 0;

    n_need_enqueue = N;
    n_enqueue_ok = rte_ring_mp_enqueue_bulk(r, obj_table, n_need_enqueue, &n_empty_cells);
    if (n_enqueue_ok < N && n_empty_cells > 0) {
        n_need_enqueue = n_empty_cells;
        n_empty_cells = 0;
        n_enqueue_ok = rte_ring_mp_enqueue_bulk(r, obj_table, n_need_enqueue, &n_empty_cells);
    }
    printf("n_ok = %d\n",(int)n_enqueue_ok);

    return 0;
}

int main(int real_argc, char *real_argv[])
{
    int ret;
    unsigned i;
    struct my_thrd_args_ thrd_args = {NULL};

    int default_argc = 2;
    char *default_argv[2] = {
        real_argv[0],
        "--no-huge", // 默认不申请大页内存
    };
    int argc = default_argc;
    char **argv = default_argv;

    if (real_argc > 1) {
        argc = real_argc;
        argv = real_argv;
    }
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_panic("Cannot init EAL\n");
    }

    thrd_args.r = rte_ring_create("", 16, 0, 0);
    if (NULL == thrd_args.r) {
        printf("Error: rte_ring_create() failed\n");
        return 0;
    }

    /* call lcore_hello() on every slave lcore */
    const unsigned first_slave_id =
            rte_get_next_lcore(-1, 1, 0);
    if (first_slave_id < RTE_MAX_LCORE) {
    }
    for (i = first_slave_id; i < RTE_MAX_LCORE; i = rte_get_next_lcore(i, 1, 0)) {
        rte_eal_remote_launch(lcore_hello, &thrd_args, i);
    }
    /* call it on master lcore too */
    lcore_hello(&thrd_args);

    rte_eal_mp_wait_lcore();

    unsigned int n_available_items_left = 0;
    while (rte_ring_count(thrd_args.r) > 0){
        typedef void *      obj_ptr_t;
        #define VECTOR_MAX_ITEMS 4
        obj_ptr_t out_vector[VECTOR_MAX_ITEMS];
        unsigned int n_dequeue_ok;
        n_dequeue_ok =
            rte_ring_sc_dequeue_bulk(thrd_args.r, out_vector, VECTOR_MAX_ITEMS,
                                     &n_available_items_left);
        if (n_available_items_left > 0 && 0 == n_dequeue_ok) {
            const unsigned last_n_items = n_available_items_left;
            n_dequeue_ok =
                rte_ring_sc_dequeue_bulk(thrd_args.r, out_vector, last_n_items,
                                         &n_available_items_left);
        }
        for (unsigned int i=0; i<n_dequeue_ok; i++) {
            printf("Main(): string dequeued: %s\n", (char*) out_vector[i]);
        }
    }
    rte_ring_empty(thrd_args.r);
    rte_ring_free(thrd_args.r);
    return 0;
}
