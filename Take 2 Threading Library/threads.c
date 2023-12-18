#include <pthread.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include "threadsHelpers.h"

#define JB_RBX 0 
#define JB_RBP 1 
#define JB_R12 2 
#define JB_R13 3 
#define JB_R14 4 
#define JB_R15 5 
#define JB_RSP 6 
#define JB_PC 7

#define STACK_SIZE 32767

struct sigaction sa;
void schedule();
int id = 0; //id of current thread, main thread is 0, increment by 1 for each new thread
pthread_t gCurrent;
struct tcb *head = NULL; //head of linked list, main thread
struct tcb *running = NULL; //current running thread
int terminated = 0; //will be deleted


enum ThreadState {
     READY,
     RUNNING,
     BLOCKED,
     TERMINATED
};



struct tcb {
     pthread_t tid;
     enum ThreadState state;
     void *stack;//stack pointer
     void *args;
     struct tcb *next;
     struct tcb *prev;
     jmp_buf context;

};


int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
     
     struct tcb *thisThread = (struct tcb *)malloc(sizeof(struct tcb)); 
     id++;

     if (thisThread == NULL) {
          return -1;
     }

     if (head == NULL){
          struct tcb *mainThread = (struct tcb *)malloc(sizeof(struct tcb)); 

          head = mainThread;
          
          mainThread->next = thisThread;
          mainThread->prev = thisThread;
          mainThread->state = RUNNING;
          running = mainThread;
          mainThread->tid = (pthread_t) 0;
          gCurrent = mainThread->tid;
          
          thisThread->next = mainThread;
          thisThread->prev = mainThread;

          //initilize signal handler for scheduler
          sigemptyset(&sa.sa_mask);
          sa.sa_handler = schedule;
          sa.sa_flags = SA_NODEFER;
          sigemptyset(&sa.sa_mask);
          sigaction(SIGALRM, &sa, NULL);
          ualarm(50000, 50000);  // Send SIGALRM every 50ms
     }else{
          struct tcb *tail = head->prev;
          
          tail->next = thisThread;
          thisThread->prev = tail;
          thisThread->next = head;
          head->prev = thisThread;
     }
     
     thisThread->tid = (pthread_t)id;
     thisThread->args = arg;
     thisThread->state = READY;
     *thread = (pthread_t)id;
          
     thisThread->context[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long )start_thunk);
     thisThread->context[0].__jmpbuf[JB_R13] = (unsigned long ) arg;  
     thisThread->context[0].__jmpbuf[JB_R12] = (unsigned long ) start_routine;
     thisThread->stack = malloc(STACK_SIZE);

     void * end_of_stack = thisThread->stack + STACK_SIZE;

     void * sp = end_of_stack - sizeof(void *) - 1;
     *(unsigned long *)(sp) = (unsigned long)pthread_exit; //thank you chunhua

     thisThread->context[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long )sp);

     return 0;
}

void schedule(){
     int sj = 0;
     if(running->state == RUNNING){ 
          running->state = READY;
          sj = setjmp(running->context); //save the context of the running thread
     }
     if (sj == 0){
          struct tcb *ptr = running -> next; //stagger the schedule
          struct tcb *end = ptr;
          while(ptr->state != READY){
               
               ptr = ptr->next;
               if(ptr == end){
                    exit(0);
               }
          }
               
          struct tcb *ready = ptr;
 
          ready->state = RUNNING;
          gCurrent = ready->tid;
          ready->state = RUNNING;
          running = ready;
          longjmp(ready->context, 1);
     }

}


pthread_t pthread_self(void) {
     return gCurrent;
}

void pthread_exit(void *value_ptr){

     running->state = TERMINATED; //terminate the running thread
     //free(running->stack);
     while(1){}
     
}

