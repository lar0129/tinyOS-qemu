#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <threads.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

// Use semaphore to implement conditional variable.
// Producer-consumer problem based on conditional variable

int BUF_SIZE = 32;
int PRODUCER_NUM = 10;
int CONSUMER_NUM = 10;

int str2int(char* str){
    int res = 0, flag = 0;
    char* p = str;
    if(*p == '-'){
        flag = 1;
        p++;
    }
    else if(*p == '+'){
        p++;
    }

    while(*p){
        res = res * 10 + *p - '0';
        p++;
    }

    return flag ? -res : res;
}

void produce_one() {
    usleep(rand() % 5 * 1000 + rand() % 10);
}

void consume_one() {
    usleep(rand() % 5 * 1000 + rand() % 10);
}

/* Producer-Consumer Problem */

mtx_t mutex;
cnd_t cv;
int cnt = 0;

#define PRODUCER_COND (cnt < BUF_SIZE)
#define CONSUMER_COND (cnt > 0)

void producer(int id) {
    while (1) {
        produce_one();
        mtx_lock(&mutex);
        while(!(PRODUCER_COND)){
            cnd_wait(&cv, &mutex);
        }

        assert(PRODUCER_COND);
        // put to buffer
        printf("(");
        fflush(stdout);
        cnt++;
        //printf("producer %d: produce, cnt_after_produce == %d\n", id, cnt);
        
        cnd_broadcast(&cv);
        mtx_unlock(&mutex);
    }
}

void consumer(int id) {
    while (1) {
        mtx_lock(&mutex);
        while(!(CONSUMER_COND)){
            cnd_wait(&cv, &mutex);
        }
        
        assert(CONSUMER_COND);

        // take from buffer
        // printf("consumer %d: consume\n", id);
        printf(")");
        fflush(stdout);
        cnt--;

        cnd_broadcast(&cv);
        mtx_unlock(&mutex);

        consume_one();
    }
}

void *producer_fn(void *args){
    int i = *(int *)args;
    srand(i + 1);
    producer(i);
}

void *consumer_fn(void *args){
    int i = *(int *)args;
    srand(~i);
    consumer(i);
}

int main(int argc, char* argv[]) {
    if(argc != 4){
        printf("Usage: condition [producer num] [consumer num] [buffer size]");
        return -1;
    }

    PRODUCER_NUM = str2int(argv[1]);
    CONSUMER_NUM = str2int(argv[2]);
    BUF_SIZE = str2int(argv[3]);
    assert(PRODUCER_NUM > 0 && CONSUMER_NUM > 0 && BUF_SIZE > 0);

    mtx_init(&mutex, 0);
    cnd_init(&cv);

    pthread_t *producers = (pthread_t *)malloc(sizeof(pthread_t) * PRODUCER_NUM);
    pthread_t *consumers = (pthread_t *)malloc(sizeof(pthread_t) * CONSUMER_NUM);

    int *pids = (int *)malloc(sizeof(int) * PRODUCER_NUM);
    for(int i = 0; i < PRODUCER_NUM; i++) pids[i] = i;
    int *cids = (int *)malloc(sizeof(int) * CONSUMER_NUM);
    for(int i = 0; i < CONSUMER_NUM; i++) cids[i] = i;

    printf("producer-consumer start\n");
    for (int i = 0; i < PRODUCER_NUM; ++i) {
        void *pid = (void *)(pids + i);
        pthread_create(producers + i, NULL, producer_fn, pid);
    }
    for (int i = 0; i < CONSUMER_NUM; ++i) {
        void *cid = (void *)(cids + i);
        pthread_create(consumers + i, NULL, consumer_fn, cid);
    }
    while (1) ;
    return 0;
}
