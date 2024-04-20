#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <threads.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#define bool int
#define true 1
#define false 0

// Use semaphore to implement conditional variable.
// Producer-consumer problem based on conditional variable

int PHILOSOPHER_NUM = 5;
bool *avail = NULL;

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

void think() {
    usleep(rand() % 5 * 1000 + rand() % 10);
}

void eat() {
    usleep(rand() % 5 * 1000 + rand() % 10);
}

/* Producer-Consumer Problem */

mtx_t mutex;
cnd_t cv;
int cnt = 0;

#define PHILOSOPHER_COND(lhf, rhf) (avail[lhf] && avail[rhf]) // 左右手边的筷子都可用

void TPhilosopher(int id) {
    int lhf = id, rhf = (id + 1) % PHILOSOPHER_NUM;
    while (1) {
        think();
        mtx_lock(&mutex);
        while(!(PHILOSOPHER_COND(lhf, rhf))){
            cnd_wait(&cv, &mutex);
        }

        assert(PHILOSOPHER_COND(lhf, rhf));

        /*Change conditions: philosopher eat*/
        avail[lhf] = avail[rhf] = false;

        /*Do something: philosopher eat*/
        printf("Philosopher %d begins eating.\n", id);
        fflush(stdout);
        eat();
        printf("Philosopher %d ends eating.\n", id);
        fflush(stdout);
        
        /*Change conditions: philosopher finish eating*/
        avail[lhf] = avail[rhf] = true;
        
        cnd_broadcast(&cv);
        mtx_unlock(&mutex);
    }
}

void *philosopher_fn(void *args){
    int i = *(int *)args;
    srand(i + 1);
    TPhilosopher(i);
    return (void *)0;
}

int main(int argc, char* argv[]) {
    if(argc != 2){
        printf("Usage: condition [philosopher number]");
        return -1;
    }

    PHILOSOPHER_NUM = str2int(argv[1]);
    assert(PHILOSOPHER_NUM > 0);

    printf("There are %d philosophers.\n", PHILOSOPHER_NUM);

    mtx_init(&mutex, 0); // 初始化锁
    cnd_init(&cv); // 初始化条件变量
    avail = (bool *)malloc(sizeof(bool) * PHILOSOPHER_NUM); // 筷子是否可用
    for(int i = 0; i < PHILOSOPHER_NUM; i++) avail[i] = true;
    int *ids = (int *)malloc(sizeof(int) * PHILOSOPHER_NUM); // 哲学家的编号
    for(int i = 0; i < PHILOSOPHER_NUM; i++) ids[i] = i;

    pthread_t *philosophers = (pthread_t *)malloc(sizeof(pthread_t) * PHILOSOPHER_NUM); // 哲学家线程

    printf("producer-consumer start\n");
    for (int i = 0; i < PHILOSOPHER_NUM; i++) {
        printf("In the loop: %d\n", i);
        void *thread_id = (void *)(ids + i);
        pthread_create(philosophers + i, NULL, philosopher_fn, thread_id);
    }

    while (1) ;
    return 0;
}