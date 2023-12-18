#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define THREAD_CNT 3
int join = 0;
void mysleep(int i) {
    unsigned long int j = 0;
    while(j < i * 1000000) {j++;}
}

// waste some time
void *func(void *arg) {
    printf("thread %lu: arg = %lu\n", (unsigned long)pthread_self(), (unsigned long)arg);
    unsigned long i;
    for(i = 0; i < 2; i++) {
        printf("thread %lu: print\n", (unsigned long)pthread_self());
        //pause();
        if((unsigned long)pthread_self() == 1 && join == 0 && i == 1&& 0){
            join = 1;
            pthread_t to_join = (pthread_t)3;
            printf("thread %lu: wait for thead %d\n", (unsigned long)pthread_self(), (int)to_join);
            pthread_join(to_join, NULL);
            
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    pthread_t threads[THREAD_CNT];
    unsigned long i;

    //create THREAD_CNT threads
    for(i = 0; i < THREAD_CNT; i++) {
        pthread_create(&threads[i], NULL, func, (void *)(i + 100));
        printf("thread %lu created\n", (unsigned long)(threads[i]));
    }

    for(i = 0; i < 4; i++) {
        printf("thread main: print\n");
        //pause();
        mysleep(100);
    }

    return 0;
}
