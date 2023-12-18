#include <pthread.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int tls_create(unsigned int size);
int tls_write(unsigned int offset, unsigned int length, char *buffer);
int tls_read(unsigned int offset, unsigned int length, char *buffer);
int tls_destroy();
int tls_clone(pthread_t tid);
void tls_init();
void tls_handle_page_fault();


typedef struct thread_local_storage{
	pthread_t tid;
	unsigned int size; /* size in bytes */
	unsigned int page_num; /* number of pages */
	struct page **pages; /* array of pointers to pages */
	struct thread_local_storage * next;
} TLS;

struct page {
	unsigned long int address; /* start address of page */
	int ref_count; /* counter for shared pages */
};

void tls_protect(struct page *p);
void tls_unprotect(struct page *p);

TLS *head = NULL;
unsigned int page_size;
int initialized = 0;

int tls_create(unsigned int size){
	
	pthread_t curentId = pthread_self();
	if(head == NULL){
		//init
		if(initialized == 0){
			tls_init();
			initialized = 1;
		}
		head = calloc(1, sizeof(TLS));
		head->next = NULL;
	}else{
		TLS* ptr = head;
		while(ptr != NULL){
			//check if tlb exist for this thread
			if(ptr->tid == curentId) {
				return -1;
			}
			ptr = ptr->next;
		}
		//create new to the front for linked list
		TLS *storage = calloc(1, sizeof(TLS));
		storage->next = head;
		head = storage;
		
	}
	//head is the new inserted
	head->tid = curentId;

	//allocate memory
	//head->page_num = (int) size/page_size; //this is wrong, rounding down
	head->page_num = (size + page_size - 1) / page_size; //round up
	head->pages = (struct page**)calloc(1, head->page_num * sizeof(struct page*));
	//head->size = head->page_num * page_size;
	
	head->size = size; //not alligned with end of page
	int i;
	for(i = 0; i < head->page_num; i++){
		struct page *p = calloc(1, sizeof(struct page));
		p->address = (unsigned long int)mmap(0,page_size,0, MAP_ANON | MAP_PRIVATE, 0, 0);
		p->ref_count = 1;
		if(p->address == -1) {
			TLS* tmp = head;
			head = head->next;
			free(tmp);
			return -1;
		}
		tls_protect(p);;
		head->pages[i] = p;

	}
	return 0;
}

int tls_write(unsigned int offset, unsigned int length, char *buffer){
	//find tls
	TLS* current = head;
	pthread_t curentId = pthread_self();
	while(current != NULL && current->tid != curentId){
		current = current->next;
	}
	if(current == NULL){
		return -1; //tlb not exist
	}
	if(offset + length > current->size){
		return -1;//out of bounds
	}
	int i;
	for(i = 0; i < current->page_num; i++){
		tls_unprotect(current->pages[i]);
	}//open permission

	int cnt, idx;
	for (cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx) {
		
		struct page *p, *copy;
		unsigned int pageNum, pageoff;
		pageNum = idx / page_size;
		pageoff = idx % page_size;
		p = current->pages[pageNum];
		if (p->ref_count > 1) {
			/* this page is shared, create a private copy (COW) */
			copy = (struct page *) calloc(1, sizeof(struct page));
			copy->address = (unsigned long int) mmap(0, page_size, PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
			tls_unprotect(copy);
			copy->ref_count = 1;
			current->pages[pageNum] = copy;
			/* update original page */
			p->ref_count--;
			/* copy original contents to copy */
			memcpy((void *) copy->address, (void *) p->address, page_size);
			/* update pointer */
			tls_protect(p);
			p = copy;
			current->pages[pageNum] = p;
		}
		char* dst = ((char *) p->address) + pageoff;
		*dst = buffer[cnt];

		
	}
	
	for(i = 0; i < current->page_num; i++){
		tls_protect(current->pages[i]);
	}//close permission
	
	return 0;
}
int tls_read(unsigned int offset, unsigned int length, char *buffer){
	//find tls
	TLS* current = head;
	pthread_t curentId = pthread_self();
	while(current != NULL && current->tid != curentId){
		current = current->next;
	}
	if(current == NULL){
		return -1; //tlb not exist
	}
	if(offset + length > current->size){
		return -1;//out of bounds
	}
	int i;
	for(i = 0; i < current->page_num; i++){
		tls_unprotect(current->pages[i]);
	}//open permission
	int cnt, idx;
	for (cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx) {
		struct page *p;
		unsigned int pageNum, pageoff;
		pageNum = idx / page_size;
		pageoff = idx % page_size;
		p = current->pages[pageNum];
		char* scr = ((char *) p->address) + pageoff;
		buffer[cnt] = *scr;
	}
	for(i = 0; i < current->page_num; i++){
		tls_protect(current->pages[i]);
	}//close permission
	return 0;
}
int tls_destroy(){
	pthread_t curentId = pthread_self();
	TLS* current = head;
	while(current != NULL){
		if(current->tid == curentId){
			break;
		}
		current = current->next;
	}
	if(current == NULL){
		return -1; //tlb not exist
	}

	int i;
	for(i = 0; i < current->page_num; i++){
		current->pages[i]->ref_count--;
		if(current->pages[i]->ref_count == 0){
			munmap((void *) current->pages[i]->address, page_size);
			//free(current->pages[i]);
		}
	}
	free(current->pages);
	//remove from linked list

	if(current == head){ //edge case
		if(head->next != NULL){
			head = head->next;
		}else{
			head = NULL;
		}
		free(current);
		return 0;
	}

	TLS* tmp = head;
	while(tmp->next != current){
		tmp = tmp->next;
	}
	//tail case
	if(current->next == NULL){
		tmp->next = NULL;
		free(current);
		return 0;
	}
	tmp->next = current->next;
	free(current);

	return 0;
}
int tls_clone(pthread_t tid){
	pthread_t curentId = pthread_self();
	TLS* current = head;
	while(current != NULL){
		if(current->tid == curentId){
			return -1; //tlb already exist
		}
		current = current->next;
	}
	//find the tls to clone
	current = head;
	while(current != NULL){
		if(current->tid == tid){
			break;
		}
		current = current->next;
	}
	if(current == NULL){
		return -1; //tlb not exist
	}
	//create new to the front for linked list
	TLS *storage = calloc(1, sizeof(TLS));
	storage->next = head;
	head = storage;
	//head is the new inserted
	head->tid = curentId;
	head->size = current->size;
	head->page_num = current->page_num;
	//create a copy of the pages pointers
	head->pages = calloc(1, head->page_num * sizeof(struct page*));
	int i;
	for(i = 0; i < head->page_num; i++){
		head->pages[i] = current->pages[i];
		head->pages[i]->ref_count++;
	}
	
	return 0;
}

void tls_init()
{
	struct sigaction sigact;
	/* get the size of a page */
	page_size = getpagesize();
	/* install the signal handler for page faults (SIGSEGV, SIGBUS) */
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_SIGINFO; /* use extended signal handling */
	sigact.sa_sigaction = tls_handle_page_fault;
	sigaction(SIGBUS, &sigact, NULL);
	sigaction(SIGSEGV, &sigact, NULL);
}

void tls_handle_page_fault(int sig, siginfo_t *si, void *context) {
    unsigned long int faulting_address = ((unsigned long int)si->si_addr) & ~(page_size - 1);

    // Check if the faulting address is within any TLS region for the current thread
	int tls_violation = 0;
	TLS* current = head;
	while(current != NULL){
		int i;
		for(i = 0; i < current->page_num; i++){
			if(current->pages[i]->address == faulting_address){
				tls_violation = 1;
				break;
			}
		}
		current = current->next;
	}
    if (tls_violation) {
        // TLS violation
        pthread_exit(NULL);
    } else {
        // Regular fault
        signal(sig, SIG_DFL);
        raise(sig);
    }
}

void tls_protect(struct page *p)
{
	if (mprotect((void *) p->address, page_size, 0)) {
		fprintf(stderr, "tls_protect: could not protect page\n");
		exit(1);
	}
}

void tls_unprotect(struct page *p)
{
	if (mprotect((void *) p->address, page_size, \
		PROT_READ | PROT_WRITE)) {
		fprintf(stderr, "tls_unprotect: could not unprotect page\n");
		exit(1);
	}
}
