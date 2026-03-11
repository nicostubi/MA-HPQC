#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX 100000000

typedef struct {
	int veut[2];
	int tour;
} petmutex;

void petmutex_lock(petmutex* p, int thr)
{
	int autre = 1 - thr;
	p->veut[thr] = 1;
	p->tour = autre;
	while (p->veut[autre] == 1 && p->tour == autre);
}

void petmutex_unlock(petmutex* p, int thr)
{
	p->veut[thr] = 0;
}

petmutex mut;
long zeglobale = 0;


// Optional: pin a thread to a CPU (0 or 1, Linux).
static void pin_to_cpu(int pin) {
#ifdef __linux__
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET((unsigned)(pin & 1), &set);
    sched_setaffinity(0, sizeof(set), &set); // ignore errors
#else
    (void)pin;
#endif
}


void* func(void *p)
{
	int moi = (long) p;
	for (int i = 0; i < MAX; i ++) {
		petmutex_lock(&mut, moi);
		int z = zeglobale + 1;
		for (int k = 0; k < 20; ++k) { /* noop */ }
		zeglobale = z;
		petmutex_unlock(&mut, moi);
	}
	return 0;
}

int main()
{
    pthread_t p1, p2;
    pthread_create(&p1, NULL, func, (void*)0);
    pthread_create(&p2, NULL, func, (void*)1);
    pthread_join(p1, NULL);
    pthread_join(p2, NULL);
    printf("missed increments = %ld\n", (MAX + MAX - zeglobale));
    return 0;
}
