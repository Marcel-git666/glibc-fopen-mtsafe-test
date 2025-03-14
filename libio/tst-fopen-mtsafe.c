#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

/* If we compile outside glibc, we don't have access to support libraries */
#ifndef STANDALONE_TEST
#include <support/check.h>
#include <support/xthread.h>
#endif

#define NUM_THREADS 16
#define FILENAME_MAXLEN 100
#define NUM_ITERATIONS 10

struct thread_data
{
  int thread_id;
  int test_passed;
};

static pthread_barrier_t barrier;

static int
run_file_creation_test(struct thread_data *data, const char *filename)
{
  FILE *file = NULL;

  file = fopen(filename, "w");
  if (file == NULL)
    {
      printf("error: Thread %d: Failed to create test file %s\n",
            data->thread_id, filename);
      return 0;
    }

  if (fprintf(file, "%d", data->thread_id) < 0)
    {
      printf("error: Thread %d encountered error writing to file: %s\n",
             data->thread_id, strerror(errno));
      fclose(file);
      return 0;
    }
  fclose(file);
  return 1;
}

static int
run_nonexistent_file_test(struct thread_data *data)
{
  char nonexistent_filename[FILENAME_MAXLEN];
  snprintf(nonexistent_filename, FILENAME_MAXLEN,
          "nonexistent_file_%d.xyz", data->thread_id);

  FILE *nonexistent = fopen(nonexistent_filename, "r");
  if (nonexistent != NULL)
    {
      printf("error: Nonexistent file exists!\n");
      fclose(nonexistent);
      return 0;
    }

  /* Store errno just after call */
  int my_errno = errno;

  if (my_errno != ENOENT)
    {
      printf("error: Thread %d: Unexpected errno value: %d\n",
             data->thread_id, my_errno);
      return 0;
    }

  return 1;
}

static int
run_file_verification_test(struct thread_data *data, const char *filename)
{
  FILE *file = NULL;

  file = fopen (filename, "r");
  if (file == NULL)
    {
      printf ("error: Thread %d: Failed to reopen test file\n",
             data->thread_id);
      return 0;
    }

  int stored_id;
  if (fscanf (file, "%d", &stored_id) != 1 || stored_id != data->thread_id)
    {
      printf ("error: Thread %d: can't verify file content\n", data->thread_id);
      return 0;
    }
  if (file != NULL)
    fclose(file);
  return 1;
}

static void
test_basic_mtsafety (struct thread_data *data)
{
  char unique_filename[FILENAME_MAXLEN];

  /* Each thread creates its unique file */
  snprintf (unique_filename, FILENAME_MAXLEN, "mtsafe_thread_%d.txt",
           data->thread_id);

  if (run_file_creation_test(data, unique_filename)
      && run_nonexistent_file_test(data)
      && run_file_verification_test(data, unique_filename))
    data->test_passed++;

  unlink (unique_filename);
}

/* Function run by threads */
static void *
thread_function (void *arg)
{
  struct thread_data *data = (struct thread_data *) arg;
  int j;

  pthread_barrier_wait (&barrier);

  /* Perform test multiple times to increase stress */
  for (j = 0; j < NUM_ITERATIONS; j++)
    test_basic_mtsafety(data);

  return NULL;
}

static int
create_and_run_threads (pthread_t *threads, struct thread_data *thread_data)
{
  int i;
  int ret;

  for (i = 0; i < NUM_THREADS; i++)
    {
      thread_data[i].thread_id = i;
      thread_data[i].test_passed = 0;

      ret = pthread_create (&threads[i], NULL, thread_function, &thread_data[i]);
      if (ret != 0)
        {
          printf ("error: Can't create a thread %d: %s\n", i, strerror (ret));
          return 0;
        }
    }

  return 1;
}

static int
join_threads (pthread_t *threads)
{
  int i;
  int ret;

  for (i = 0; i < NUM_THREADS; i++)
    {
      ret = pthread_join (threads[i], NULL);
      if (ret != 0)
        {
          printf ("error: Can't join a thread %d: %s\n", i, strerror (ret));
          return 0;
        }
    }

  return 1;
}

static int
report_results (const struct thread_data *thread_data)
{
  int i;
  int total_passes = 0;
  int expected_passes = NUM_THREADS * NUM_ITERATIONS;

  for (i = 0; i < NUM_THREADS; i++)
    total_passes += thread_data[i].test_passed;

  printf ("info: Tests passed: %d of %d expected\n",
          total_passes, expected_passes);

  /* Return 1 (success) only if all tests passed */
  return (total_passes == expected_passes) ? 1 : 0;
}

static int
do_test (void)
{
  pthread_t threads[NUM_THREADS];
  struct thread_data thread_data[NUM_THREADS];
  int success = 1;

  pthread_barrier_init (&barrier, NULL, NUM_THREADS);
  printf ("info: fopen() MT test starting with %d threads and %d cycles.\n",
      NUM_THREADS, NUM_ITERATIONS);

  if (!create_and_run_threads (threads, thread_data))
    {
      success = 0;
      goto cleanup_barrier;
    }

  if (!join_threads (threads))
    {
      success = 0;
      goto cleanup_barrier;
    }

  printf ("info: All threads completed\n");

  success = report_results (thread_data);

cleanup_barrier:
  pthread_barrier_destroy (&barrier);

  return success ? 0 : 1;
}

#ifdef STANDALONE_TEST
int
main (void)
{
  int result = do_test ();
  if (result == 0)
    printf ("info: TEST PASSED\n");
  else
    printf ("error: TEST FAILED (code %d)\n", result);
  return result;
}
#else
#include <support/test-driver.c>
#endif
