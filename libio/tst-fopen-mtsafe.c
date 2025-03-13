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

#define NUM_FILES 4
#define NUM_THREADS 64
#define FILENAME_MAXLEN 100
#define NUM_ITERATIONS 100
/* Struct for data and threads */
struct thread_data
{
  int thread_id;
  int test_passed;
};

static pthread_barrier_t barrier;

/* Test basic MT-Safety fopen() */
static void
test_basic_mtsafety (struct thread_data *data)
{
  char unique_filename[FILENAME_MAXLEN];
  FILE *file = NULL;
  int result = 0;

  /* Each thread creates its unique file */
  snprintf (unique_filename, FILENAME_MAXLEN, "mtsafe_thread_%d.txt",
           data->thread_id);

  /* 1st Test: File creation and writing */
  file = fopen (unique_filename, "w");
  if (file == NULL)
    {
      printf ("error: Thread %d: Failed to create test file %s\n",
             data->thread_id, unique_filename);
      goto cleanup;
    }

  /* Write Thread ID to file */
  if (fprintf (file, "%d", data->thread_id) < 0)
    {
      printf ("error: Thread %d encountered error writing to file: %s\n",
        data->thread_id, strerror (errno));
      goto cleanup;
    }

  fclose (file);
  file = NULL;

  /* 2nd Test: Open nonexistent file (test errno) */
  char nonexistent_filename[FILENAME_MAXLEN];
  snprintf (nonexistent_filename, FILENAME_MAXLEN,
           "nonexistent_file_%d.xyz", data->thread_id);

  FILE *nonexistent = fopen (nonexistent_filename, "r");
  if (nonexistent != NULL)
    {
      /* This shouldn't happen, but still we can clean */
      printf("error: Nonexistent file exists!\n");
      fclose (nonexistent);
      unlink (nonexistent_filename);
      goto cleanup;
    }
  else
    {
      /* Store errno just after call */
      int my_errno = errno;
      /* Check whether errno is set to ENOENT */
      if (my_errno != ENOENT)
        {
          printf("error: Thread %d: Unexpected errno value: %d\n",
                 data->thread_id, my_errno);
          goto cleanup;
        }
    }

  /* 3rd Test: Open its own file again */
  file = fopen (unique_filename, "r");
  if (file == NULL)
    {
      printf ("error: Thread %d: Failed to reopen test file\n",
             data->thread_id);
      goto cleanup;
    }

  int stored_id;
  if (fscanf (file, "%d", &stored_id) != 1 || stored_id != data->thread_id)
    {
      printf ("error: Thread %d: can't verify file content\n", data->thread_id);
      goto cleanup;
    }

  /* All tests passed */
  result = 1;

cleanup:
  if (file != NULL)
    fclose (file);
  unlink (unique_filename);

  /* Update thread data with result */
  if (result)
    data->test_passed++;
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
    {
      test_basic_mtsafety(data);
    }

  return NULL;
}

/* Initialize thread data and create threads */
static int
create_and_run_threads (pthread_t *threads, struct thread_data *thread_data)
{
  int i;
  int ret;

  /* Initialize thread data */
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

/* Wait for all threads to complete */
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

/* Calculate and report test results */
static int
report_results (const struct thread_data *thread_data)
{
  int i;
  int total_passes = 0;
  int expected_passes = NUM_THREADS * NUM_ITERATIONS;

  for (i = 0; i < NUM_THREADS; i++)
    {
      total_passes += thread_data[i].test_passed;
    }

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

  /* Initialize the barrier */
  pthread_barrier_init (&barrier, NULL, NUM_THREADS);
  printf ("info: fopen() MT test starting with %d threads and %d cycles.\n",
    NUM_THREADS, NUM_ITERATIONS);

  /* Create and run threads */
  if (!create_and_run_threads (threads, thread_data))
    {
      success = 0;
      goto cleanup_barrier;
    }

  /* Join threads */
  if (!join_threads (threads))
    {
      success = 0;
      goto cleanup_barrier;
    }

  printf ("info: All threads completed\n");

  /* Check and report results */
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
