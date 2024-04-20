#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
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

/* Mutex based on semaphore */

typedef struct mutex_struct
{
    sem_t lock;
} my_mutex_t;

void my_mutex_init(my_mutex_t* mutex){
    sem_init(&mutex->lock, 0, 1); // mutex = semaphore with a value that equals 1
}

void my_mutex_lock(my_mutex_t *mutex){
    int res = sem_wait(&mutex->lock); // P
    assert(res == 0);
}

void my_mutex_unlock(my_mutex_t *mutex){
    int res = sem_post(&mutex->lock); // V
}

/* Conditional Variable based on semaphore */

typedef struct cv_struct
{
    int nwait; // The number of processes(threads) waiting on the conditional variable
    sem_t sleep; // The user semaphore for processes(threads) to sleep 
    my_mutex_t lock; // mutex to protect nwait
    sem_t handshake; // The semaphore for handshaking
} my_cv_t;

void my_cv_init(my_cv_t *cv){
    cv->nwait = 0;
    sem_init(&cv->sleep, 0, 0);
    my_mutex_init(&cv->lock);
    sem_init(&cv->handshake, 0, 0);
}

void my_cv_wait(my_cv_t *cv, my_mutex_t *mutex){
    my_mutex_lock(&cv->lock);
    // Randomly wake up a waiting processes(threads).
    cv->nwait++;
    my_mutex_unlock(&cv->lock);

    my_mutex_unlock(mutex);

    // We have marked this thread as "waiting".
    // Now we release the lock and go to sleep.
    //
    // <-- broadcast() may happen here before T1 successfully waits.
    //     Suppose T2 calls broadcast() and proceeds
    //     with execution. T3 may call wait() and 
    //     erroneouosly being signaled. 

    sem_wait(&cv->sleep); // P (must after unlock mutex, otherwise deadlock)
    sem_post(&cv->handshake); //V

    my_mutex_lock(mutex);

    /*
    Summary: wait -- atomic opeartion
    1. nwait++
    2. unlock(&mutex)
    3. P(sleep)

    Solution: Handshake protocol with yet another emaphore h 
    */
}

void my_cv_signal(my_cv_t *cv, my_mutex_t *mutex){
    my_mutex_lock(&cv->lock);

    // Randomly wake up a waiting processes(threads)
    if(cv->nwait > 0){
        cv->nwait--;
        sem_post(&cv->sleep); // V
        sem_wait(&cv->handshake); // P
    }
    
    my_mutex_unlock(&cv->lock);    
}

void my_cv_broadcast(my_cv_t *cv, my_mutex_t *mutex){
    my_mutex_lock(&cv->lock);
    // Wake up all waiting processes(threads)
    for(int i = 0; i < cv->nwait; i++){
        sem_post(&cv->sleep); // V
        sem_wait(&cv->handshake); // P
    }
    cv->nwait = 0;
    my_mutex_unlock(&cv->lock);    
}

/* Producer-Consumer Problem */

my_mutex_t mutex;
my_cv_t cv;
int cnt = 0;

#define PRODUCER_COND (cnt < BUF_SIZE)
#define CONSUMER_COND (cnt > 0)

void producer(int id) {
    while (1) {
        produce_one();
        my_mutex_lock(&mutex);
        while(!(PRODUCER_COND)){
            my_cv_wait(&cv, &mutex);
        }

        assert(PRODUCER_COND);
        // put to buffer
        printf("(");
        fflush(stdout);
        cnt++;
        //printf("producer %d: produce, cnt_after_produce == %d\n", id, cnt);
        
        my_cv_broadcast(&cv, &mutex);
        my_mutex_unlock(&mutex);
    }
}

void consumer(int id) {
    while (1) {
        my_mutex_lock(&mutex);
        while(!(CONSUMER_COND)){
            my_cv_wait(&cv, &mutex);
        }
        
        assert(CONSUMER_COND);

        // take from buffer
        // printf("consumer %d: consume\n", id);
        printf(")");
        fflush(stdout);
        cnt--;

        my_cv_broadcast(&cv, &mutex);
        my_mutex_unlock(&mutex);

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

    my_mutex_init(&mutex);
    my_cv_init(&cv);

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
    while (1);
    return 0;
}
