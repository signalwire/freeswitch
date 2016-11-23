#include "ks.h"
#include "tap.h"
#define MAX 200

static void *test1_thread(ks_thread_t *thread, void *data)
{
	ks_q_t *q = (ks_q_t *) data;
	void *pop;

	while(ks_q_pop(q, &pop) == KS_STATUS_SUCCESS) {
		//int *i = (int *)pop;
		//printf("POP %d\n", *i);
		ks_pool_free(thread->pool, pop);
	}
	
	return NULL;
}

static void do_flush(ks_q_t *q, void *ptr, void *flush_data)
{
	ks_pool_t *pool = (ks_pool_t *)flush_data;
	ks_pool_free(pool, ptr);

}

int qtest1(int loops)
{
	ks_thread_t *thread;
	ks_q_t *q;
	ks_pool_t *pool;
	int i;

	ks_pool_open(&pool);
	ks_q_create(&q, pool, loops);
	
	ks_thread_create(&thread, test1_thread, q, pool);

	for (i = 0; i < 10000; i++) {
		int *val = (int *)ks_pool_alloc(pool, sizeof(int));
		*val = i;
		ks_q_push(q, val);
	}

	ks_q_wait(q);

	ks_q_term(q);
	ks_thread_join(thread);

	ks_q_destroy(&q);

	ks_q_create(&q, pool, loops);
	ks_q_set_flush_fn(q, do_flush, pool);

	for (i = 0; i < loops; i++) {
		int *val = (int *)ks_pool_alloc(pool, sizeof(int));
		*val = i;
		ks_q_push(q, val);
	}

	ks_q_destroy(&q);

	ks_pool_close(&pool);

	return 1;

}

struct test2_data {
	ks_q_t *q;
	int try;
	int ready;
	int running;
};

static void *test2_thread(ks_thread_t *thread, void *data)
{
	struct test2_data *t2 = (struct test2_data *) data;
	void *pop;
	ks_status_t status;
	int popped = 0;

	while (t2->running && (t2->try && !t2->ready)) {
		ks_sleep(10000);
		continue;
	}

	while (t2->running || ks_q_size(t2->q)) {
		if (t2->try) {
			status = ks_q_trypop(t2->q, &pop);
		} else {
			status = ks_q_pop(t2->q, &pop);
		}

		if (status == KS_STATUS_SUCCESS) {
			//int *i = (int *)pop;
			//printf("%p POP %d\n", (void *)pthread_self(), *i);
			popped++;
			ks_pool_free(thread->pool, pop);
		} else if (status == KS_STATUS_INACTIVE) {
			break;
		} else if (t2->try && ks_q_size(t2->q)) {
			int s = rand() % 100;
			ks_sleep(s * 1000);
		}
	}

	return (void *) (intptr_t)popped;
}

ks_size_t qtest2(int ttl, int try, int loops)
{
	ks_thread_t *threads[MAX];
	ks_q_t *q;
	ks_pool_t *pool;
	int i;
	struct test2_data t2 = { 0 };
	ks_size_t r;
	int dropped = 0;
	int qlen = loops / 2;
	int total_popped = 0;

	ks_pool_open(&pool);
	ks_q_create(&q, pool, qlen);

	t2.q = q;
	t2.try = try;
	t2.running = 1;

	for (i = 0; i < ttl; i++) {
		ks_thread_create(&threads[i], test2_thread, &t2, pool);
	}

	//ks_sleep(loops00);

	for (i = 0; i < loops; i++) {
		int *val = (int *)ks_pool_alloc(pool, sizeof(int));
		*val = i;
		if (try > 1) {
			if (ks_q_trypush(q, val) != KS_STATUS_SUCCESS) {
				dropped++;
			}
		} else {
			ks_q_push(q, val);
		}
		if (i > qlen / 2) {
			t2.ready = 1;
		}
	}

	t2.running = 0;

	if (!try) {
		ks_q_wait(q);
		ks_q_term(q);
	}

	for (i = 0; i < ttl; i++) {
		int popped;
		ks_thread_join(threads[i]);
		popped = (int)(intptr_t)threads[i]->return_data;
		if (popped) {
			printf("%d/%d POPPED %d\n", i, ttl, popped);
		}
		total_popped += popped;
	}

	r = ks_q_size(q);
	ks_assert(r == 0);

	ks_q_destroy(&q);



	printf("TOTAL POPPED: %d DROPPED %d SUM: %d\n", total_popped, dropped, total_popped + dropped);fflush(stdout);

	if (try < 2) {
		ks_assert(total_popped == loops);
	} else {
		ks_assert(total_popped + dropped == loops);
	}

	ks_pool_close(&pool);

	return r;

}


int main(int argc, char **argv)
{
	int ttl;
	int size = 100000;
	int runs = 1;
	int i;

	ks_init();

	srand((unsigned)(time(NULL) - (unsigned)(intptr_t)ks_thread_self()));

	plan(4 * runs);

	ttl = ks_cpu_count() * 5;
	//ttl = 5;


	for(i = 0; i < runs; i++) {
		ok(qtest1(size));
		ok(qtest2(ttl, 0, size) == 0);
		ok(qtest2(ttl, 1, size) == 0);
		ok(qtest2(ttl, 2, size) == 0);
	}

	printf("TTL %d RUNS %d\n", ttl, runs);

	ks_shutdown();

	done_testing();
}
