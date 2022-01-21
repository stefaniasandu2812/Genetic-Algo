#ifndef MY_THREAD_H
#define MY_THREAD_H

/* structure to store arguments for the thread function */
typedef struct my_thread {
	int P;
	int id;
	int object_count;
	int generations_count;
	int sack_capacity;
	const sack_object *objects;
	individual *current_generation;
	individual *next_generation;
	individual *tmp;
	pthread_barrier_t *barrier;
} my_thread;

#endif