#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "genetic_algorithm_par.h"
#include "my_thread.h"

#define MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

int read_input(sack_object **objects, int *object_count, int *sack_capacity, int *generations_count, int *P, int argc, char *argv[])
{
	FILE *fp;

	if (argc < 4) {
		fprintf(stderr, "Usage:\n\t./tema1 in_file generations_count\n");
		return 0;
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		return 0;
	}

	if (fscanf(fp, "%d %d", object_count, sack_capacity) < 2) {
		fclose(fp);
		return 0;
	}

	if (*object_count % 10) {
		fclose(fp);
		return 0;
	}

	sack_object *tmp_objects = (sack_object *) calloc(*object_count, sizeof(sack_object));

	for (int i = 0; i < *object_count; ++i) {
		if (fscanf(fp, "%d %d", &tmp_objects[i].profit, &tmp_objects[i].weight) < 2) {
			free(objects);
			fclose(fp);
			return 0;
		}
	}

	fclose(fp);

	*generations_count = (int) strtol(argv[2], NULL, 10);
	
	if (*generations_count == 0) {
		free(tmp_objects);

		return 0;
	}

	// get number of threads
	*P = (int) strtol(argv[3], NULL, 10);

	*objects = tmp_objects;

	return 1;
}

void print_objects(const sack_object *objects, int object_count)
{
	for (int i = 0; i < object_count; ++i) {
		printf("%d %d\n", objects[i].weight, objects[i].profit);
	}
}

void print_generation(const individual *generation, int limit)
{
	for (int i = 0; i < limit; ++i) {
		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			printf("%d ", generation[i].chromosomes[j]);
		}

		printf("\n%d - %d\n", i, generation[i].fitness);
	}
}

void print_best_fitness(const individual *generation)
{
	printf("%d\n", generation[0].fitness);
}

void compute_fitness_function(const sack_object *objects, individual *generation, int start, int end, int sack_capacity)
{
	int weight;
	int profit;
	int count_objects;

	for (int i = start; i < end; ++i) {
		weight = 0;
		profit = 0;
		count_objects = 0;

		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			if (generation[i].chromosomes[j]) {
				weight += objects[j].weight;
				profit += objects[j].profit;
				count_objects++;
			}
		}
		
		// update number of objects
		generation[i].nr_objects = count_objects;
		generation[i].fitness = (weight <= sack_capacity) ? profit : 0;
	}
}

int cmpfunc(const void *a, const void *b)
{
	individual *first = (individual *) a;
	individual *second = (individual *) b;

	int res = second->fitness - first->fitness; // decreasing by fitness
	if (res == 0) {

		res = first->nr_objects - second->nr_objects; // increasing by number of objects in the sack

		if (res == 0) {
			return second->index - first->index;
		}
	}

	return res;
}

void mutate_bit_string_1(const individual *ind, int generation_index)
{
	int i, mutation_size;
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	if (ind->index % 2 == 0) {
		// for even-indexed individuals, mutate the first 40% chromosomes by a given step
		mutation_size = ind->chromosome_length * 4 / 10;
		for (i = 0; i < mutation_size; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	} else {
		// for even-indexed individuals, mutate the last 80% chromosomes by a given step
		mutation_size = ind->chromosome_length * 8 / 10;
		for (i = ind->chromosome_length - mutation_size; i < ind->chromosome_length; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	}
}

void mutate_bit_string_2(const individual *ind, int generation_index)
{
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	// mutate all chromosomes by a given step
	for (int i = 0; i < ind->chromosome_length; i += step) {
		ind->chromosomes[i] = 1 - ind->chromosomes[i];
	}
}

void crossover(individual *parent1, individual *child1, int generation_index)
{
	individual *parent2 = parent1 + 1;
	individual *child2 = child1 + 1;
	int count = 1 + generation_index % parent1->chromosome_length;

	memcpy(child1->chromosomes, parent1->chromosomes, count * sizeof(int));
	memcpy(child1->chromosomes + count, parent2->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));

	memcpy(child2->chromosomes, parent2->chromosomes, count * sizeof(int));
	memcpy(child2->chromosomes + count, parent1->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));
}

void copy_individual(const individual *from, const individual *to)
{
	memcpy(to->chromosomes, from->chromosomes, from->chromosome_length * sizeof(int));
}

void free_generation(individual *generation, int start, int end)
{
	int i;

	for (i = start; i < end; ++i) {
		free(generation[i].chromosomes);
		generation[i].chromosomes = NULL;
		generation[i].fitness = 0;
	}
}

void *f (void *arg) {

	my_thread *thr_arg = (my_thread*) arg;
	int count, cursor, start, end;

	// set start - end for initial generation
	start = thr_arg->id * (double) thr_arg->object_count / thr_arg->P;
	end = MIN((thr_arg->id + 1) * (double) thr_arg->object_count / thr_arg->P, thr_arg->object_count);

	// set initial generation (composed of object_count individuals with a single item in the sack)
	for (int i = start; i < end; ++i) {
		thr_arg->current_generation[i].fitness = 0;
		thr_arg->current_generation[i].chromosomes = (int*) calloc(thr_arg->object_count, sizeof(int));
		thr_arg->current_generation[i].chromosomes[i] = 1;
		thr_arg->current_generation[i].index = i;
		thr_arg->current_generation[i].chromosome_length = thr_arg->object_count;
		thr_arg->current_generation[i].nr_objects = 0;

		thr_arg->next_generation[i].fitness = 0;
		thr_arg->next_generation[i].chromosomes = (int*) calloc(thr_arg->object_count, sizeof(int));
		thr_arg->next_generation[i].index = i;
		thr_arg->next_generation[i].chromosome_length = thr_arg->object_count;
		thr_arg->next_generation[i].nr_objects = 0;
	}

	// wait for generation
	pthread_barrier_wait(thr_arg->barrier);

	// iterate for each generation
	for (int k = 0; k < thr_arg->generations_count; ++k) {
		cursor = 0;

		start = thr_arg->id * (double) thr_arg->object_count / thr_arg->P;
		end = MIN((thr_arg->id + 1) * (double) thr_arg->object_count / thr_arg->P, thr_arg->object_count);

		// compute fitness and sort by it
		compute_fitness_function(thr_arg->objects, thr_arg->current_generation, start, end, thr_arg->sack_capacity);

		// wait to get all fitness values
		pthread_barrier_wait(thr_arg->barrier);

		if (thr_arg->id == 0) {
			qsort(thr_arg->current_generation, thr_arg->object_count, sizeof(individual), cmpfunc);
		}

		// wait to sort current generation
		pthread_barrier_wait(thr_arg->barrier);

		// keep first 30% children (elite children selection)
		count = thr_arg->object_count * 3 / 10;
		start = thr_arg->id * (double) count / thr_arg->P;
		end = MIN((thr_arg->id + 1) * (double) count / thr_arg->P, count);

		for (int i = start; i < end; ++i) {
			copy_individual(thr_arg->current_generation + i, thr_arg->next_generation + i);
		}
		cursor = count;

		// mutate first 20% children with the first version of bit string mutation
		count = thr_arg->object_count * 2 / 10;
		start = thr_arg->id * (double) count / thr_arg->P;
		end = MIN((thr_arg->id + 1) * (double) count / thr_arg->P, count);

		for (int i = start; i < end; ++i) {
			copy_individual(thr_arg->current_generation + i, thr_arg->next_generation + cursor + i);
			mutate_bit_string_1(thr_arg->next_generation + cursor + i, k);
		}
		cursor += count;

		// mutate next 20% children with the second version of bit string mutation
		count = thr_arg->object_count * 2 / 10;
		start = thr_arg->id * (double) count / thr_arg->P;
		end = MIN((thr_arg->id + 1) * (double) count / thr_arg->P, count);

		for (int i = start; i < end; ++i) {
			copy_individual(thr_arg->current_generation + i + count, thr_arg->next_generation + cursor + i);
			mutate_bit_string_2(thr_arg->next_generation + cursor + i, k);
		}
		cursor += count;

		// crossover first 30% parents with one-point crossover
		// (if there is an odd number of parents, the last one is kept as such)
		count = thr_arg->object_count * 3 / 10;

		if (count % 2 == 1) {
			copy_individual(thr_arg->current_generation + thr_arg->object_count - 1, thr_arg->next_generation + cursor + count - 1);
			count--;
		}

		start = thr_arg->id * (double) count / thr_arg->P;
		end = MIN((thr_arg->id + 1) * (double) count / thr_arg->P, count - 1);

		for (int i = start; i < end; i += 2) {
			crossover(thr_arg->current_generation + i, thr_arg->next_generation + cursor + i, k);
		}

		// wait to get the complete final next generation
		pthread_barrier_wait(thr_arg->barrier);

		// switch generations
		thr_arg->tmp = thr_arg->current_generation;
		thr_arg->current_generation = thr_arg->next_generation;
		thr_arg->next_generation = thr_arg->tmp;

		start = thr_arg->id * (double) thr_arg->object_count / thr_arg->P;
		end = MIN((thr_arg->id + 1) * (double) thr_arg->object_count / thr_arg->P, thr_arg->object_count);

		for (int i = start; i < end; ++i) {
			thr_arg->current_generation[i].index = i;
		}

		if (thr_arg->id == 0) {
			if (k % 5 == 0) {
				print_best_fitness(thr_arg->current_generation);
			}
		}
	}

	start = thr_arg->id * (double) thr_arg->object_count / thr_arg->P;
	end = MIN((thr_arg->id + 1) * (double) thr_arg->object_count / thr_arg->P, thr_arg->object_count);
	compute_fitness_function(thr_arg->objects, thr_arg->current_generation, start, end, thr_arg->sack_capacity);

	pthread_barrier_wait(thr_arg->barrier);

	if (thr_arg->id == 0) {
		qsort(thr_arg->current_generation, thr_arg->object_count, sizeof(individual), cmpfunc);
		print_best_fitness(thr_arg->current_generation);
	}

	pthread_barrier_wait(thr_arg->barrier);

	start = thr_arg->id * (double) thr_arg->current_generation->chromosome_length / thr_arg->P;
	end = MIN((thr_arg->id + 1) * (double) thr_arg->current_generation->chromosome_length / thr_arg->P, thr_arg->current_generation->chromosome_length);
	free_generation(thr_arg->current_generation, start, end);

	start = thr_arg->id * (double) thr_arg->next_generation->chromosome_length / thr_arg->P;
	end = MIN((thr_arg->id + 1) * (double) thr_arg->next_generation->chromosome_length / thr_arg->P, thr_arg->next_generation->chromosome_length);
	free_generation(thr_arg->next_generation, start, end);

	return NULL;
}

void run_genetic_algorithm(const sack_object *objects, int object_count, int generations_count, int sack_capacity, int P)
{
	int r;
	void *status;
	individual *current_generation = (individual*) calloc(object_count, sizeof(individual));
	individual *next_generation = (individual*) calloc(object_count, sizeof(individual));
	individual *tmp = NULL;

	// allocated mem for threads
	pthread_t *threads = (pthread_t*) calloc(P, sizeof(pthread_t));
	my_thread *threads_arg = (my_thread*) calloc(P, sizeof(my_thread));

	// init barrier
	pthread_barrier_t barrier;
	pthread_barrier_init(&barrier, NULL, P);


	// create array of arguments for the thread function
	for (int i = 0; i < P; ++i) {
		threads_arg[i].id = i;
		threads_arg[i].P = P;
		threads_arg[i].object_count = object_count;
		threads_arg[i].generations_count = generations_count;
		threads_arg[i].sack_capacity = sack_capacity;
		threads_arg[i].current_generation = current_generation;
		threads_arg[i].next_generation = next_generation;
		threads_arg[i].tmp = tmp;
		threads_arg[i].objects = objects;
		threads_arg[i].barrier = &barrier;

		r = pthread_create(&threads[i], NULL, f, &threads_arg[i]);

		if (r) {
			printf("Eroare la crearea thread-ului %d\n", i);
			exit(-1);
		}
	}

	// join threads
	for (int i = 0; i < P; i++) {
		r = pthread_join(threads[i], &status);

		if (r) {
			printf("Eroare la asteptarea thread-ului %d\n", i);
			exit(-1);
		}
	}

	// destroy barrier and free resources used for threads
	pthread_barrier_destroy(&barrier);
	free(threads);
	free(threads_arg);

	// free resources
	free(current_generation);
	free(next_generation);
}
