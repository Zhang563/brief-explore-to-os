#include <pthread.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <semaphore.h>
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
sigset_t block, noBlock;
void schedule();
int id = 0; //id of current thread, main thread is 0, increment by 1 for each new thread
pthread_t gCurrent;
struct tcb *head = NULL; //head of linked list, main thread
struct tcb *running = NULL; //current running thread
int terminated = 0; //will be deleted

void lock();
void unlock();
int pthread_join(pthread_t thread, void **value_ptr);
void pthread_exit(void *value_ptr);
void pthread_exit_wrapper();
int sem_init(sem_t *sem, int pshared, unsigned value);
int sem_wait(sem_t *sem);
int sem_post(sem_t *sem);
int sem_destroy(sem_t *sem);

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
     struct tcb *join;
     struct tcb *next_in_queue;
     jmp_buf context;
     void *exit_code;
};

struct my_sem_t{
     int value;
     struct tcb *queue;
     int initialized;
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
          mainThread->join = NULL;
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
     
     //printf("id: %d\n", id);
     thisThread->tid = (pthread_t)id;
     thisThread->join = NULL;
     //thisThread->stack = malloc(STACK_SIZE);
     thisThread->args = arg;
     thisThread->state = READY;
     *thread = (pthread_t)id;

          
          thisThread->context[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long )start_thunk);
          thisThread->context[0].__jmpbuf[JB_R13] = (unsigned long ) arg;  
          thisThread->context[0].__jmpbuf[JB_R12] = (unsigned long ) start_routine;
          thisThread->stack = malloc(STACK_SIZE);

          void * end_of_stack = thisThread->stack + STACK_SIZE;

          void * sp = end_of_stack - sizeof(void *) - 1;
          *(unsigned long *)(sp) = (unsigned long)pthread_exit_wrapper; 




          thisThread->context[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long )sp);

          //printf("Stack pointer (sp): %p\n", sp);

          
     

     return 0;
}



void schedule(){
     int sj = 0;
     if(running->state == RUNNING){ 
          running->state = READY;
          sj = setjmp(running->context); //save the context of the running thread
     }else if (running->state == BLOCKED){
          sj = setjmp(running->context); //save the context of the blocked thread
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
               
          //printf("running: %d\n", running->tid);
          struct tcb *ready = ptr;
          //some test
          // printf("running: %d\n", (int) running->tid);
          // printf("ready: %d\n", (int)ready->tid);
          
          
          
               
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

void pthread_exit_wrapper()
   {
       unsigned long int res;
       asm("movq %%rax, %0\n":"=r"(res));
       pthread_exit((void *) res);
}

void pthread_exit(void *value_ptr){

     running->state = TERMINATED; //terminate the running thread
     //printf("terminated\n");
     if(value_ptr != NULL)running->exit_code = value_ptr;
     if(running->join != NULL){
          //printf("unblocked\n");
          running->join->state = READY;
     }
     free(running->stack);
     //schedule();
     while(1){}
     
}

void lock(){
     
     sigemptyset(&block);//creates an empty set of signal mask
     sigaddset(&block, SIGALRM);
     //BLOCK SIGALRM
     sigprocmask(SIG_BLOCK,&block,&noBlock);

}
void unlock(){
     //UNBLOCK SIGALRM       
     sigemptyset(&noBlock);//creates an empty set of signal mask
     sigaddset(&noBlock, SIGALRM);
     //unBLOCK SIGALRM
     sigprocmask(SIG_UNBLOCK,&noBlock,NULL);
}

int pthread_join(pthread_t thread, void **value_ptr) {
     lock();
     struct tcb *ptr = head;
     struct tcb *caller = running;
     while (ptr->tid != thread) {
          ptr = ptr->next;
     }
     if (ptr->state != TERMINATED && ptr->join == NULL) {
          caller->state = BLOCKED;
          ptr->join = caller;
          //schedule();
     }
     //schedule();

     //following code will be executed after the thread is not blocked
     //while(ptr->state != TERMINATED){
     unlock();
     schedule();
     //}
     if(value_ptr!= NULL && ptr->exit_code != NULL) *value_ptr = ptr->exit_code; // set the exit code of the terminated thread
     
     return 0;
}



int sem_init(sem_t *sem, int pshared, unsigned int value) {
     struct my_sem_t *my_sem = malloc(sizeof(struct my_sem_t));
     if (my_sem == NULL) {
          return -1;
     }
     my_sem->value = value;
     my_sem->queue = NULL;
     my_sem->initialized = 1;
     sem->__align = (long int) my_sem;
     return 0;
}

int sem_destroy(sem_t *sem) {
     struct my_sem_t *my_sem = (struct my_sem_t *) sem->__align;
     if (my_sem == NULL || !my_sem->initialized) {
          return -1;
     }
     if (my_sem->queue != NULL) {
          return -1;
     }
     free(my_sem);
     sem->__align = 0;
     return 0;
}

int sem_wait(sem_t *sem) {
     lock();
     struct my_sem_t *my_sem = (struct my_sem_t *) sem->__align;
     if (my_sem == NULL || !my_sem->initialized) {
          unlock();
          return -1;
     }
     
     if(my_sem->value == 0){
          running->state = BLOCKED;
          struct tcb *ptr = my_sem->queue;
          if (ptr == NULL) {
               my_sem->queue = running;
          } else {
               while (ptr->next_in_queue != NULL) {
                    ptr = ptr->next_in_queue;
               }
               ptr->next_in_queue = running;
          }
          unlock();
          schedule();
     }
     
     if (my_sem->value > 0) {
          my_sem->value--;
          
          unlock();
     }
     return 0;
}

int sem_post(sem_t *sem) {
     lock();
     struct my_sem_t *my_sem = (struct my_sem_t *) sem->__align;
     if (my_sem == NULL || !my_sem->initialized) {
          unlock();
          return -1;
     }
     
     my_sem->value++;
     if(my_sem->value == 1){
          struct tcb *ptr = my_sem->queue;
          while(ptr != NULL && ptr->state != BLOCKED){
               
               ptr = ptr->next_in_queue;
          }
          if(ptr != NULL){
               ptr->state = READY;
               my_sem->queue = ptr->next_in_queue; //new queue head
               ptr->next_in_queue = NULL;    //remove next pointer from the unblocked thread
          }
     }
    
     unlock();
     return 0;
}
