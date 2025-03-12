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

/* Struct for data and threads */
struct thread_data
{
  int thread_id;
  int success_count;
  int error_count;
};

static pthread_barrier_t barrier;

/* Test basic MT-Safety fopen() */
static void
test_basic_mtsafety (struct thread_data *data)
{
  char unique_filename[FILENAME_MAXLEN];
  FILE *file;

  /* Each thread creates its unique file */
  snprintf (unique_filename, FILENAME_MAXLEN, "mtsafe_thread_%d.txt",
           data->thread_id);

  printf ("Thread %d testing basic MT-Safety with file %s\n",
         data->thread_id, unique_filename);

  /* 1st Test: File creation */
  file = fopen (unique_filename, "w");
  if (file != NULL)
    {
      /* Write Thread ID to file */
      if (fprintf (file, "%d", data->thread_id) < 0)
        {
        printf ("Thread %d: Error writing to file: %s\n",
          data->thread_id, strerror (errno));
        data->error_count++;
        }
      else
        {
      data->success_count++;
        }
      fclose (file);
      data->success_count++;

      /* 2nd Test: Open nonexistent file (test errno) */
      char nonexistent_filename[FILENAME_MAXLEN];
      snprintf (nonexistent_filename, FILENAME_MAXLEN,
               "nonexistent_file_%d.xyz", data->thread_id);

      FILE *nonexistent = fopen (nonexistent_filename, "r");
      if (nonexistent == NULL)
        {
          /* Store errno just after call */
          int my_errno = errno;
          /* Check whether errno is set to ENOENT */
          if (my_errno == ENOENT)
            {
              printf ("Thread %d: errno correctly set to ENOENT\n", data->thread_id);
              data->success_count++;
            }
        }
      else
        {
          /* This shouldn't happen, but still we can clean */
          printf("Unknown error!!!!");
          fclose (nonexistent);
          unlink (nonexistent_filename);
        }

      /* 3rd Test: Open its own file again */
      file = fopen (unique_filename, "r");
      if (file != NULL)
        {
          int stored_id;
          if (fscanf (file, "%d", &stored_id) == 1 && stored_id == data->thread_id)
            {
              printf ("Thread %d: Successfully verified file content\n", data->thread_id);
              data->success_count++;
            }
          fclose (file);
        }

      /* Delete test file */
      unlink (unique_filename);
    }
  else
    {
      printf ("Error: Thread %d: Failed to create test file %s\n",
             data->thread_id, unique_filename);
      data->error_count++;
    }
}

/* Function run by threads */
static void *
thread_function (void *arg)
{
  struct thread_data *data = (struct thread_data *) arg;
  int j;

  pthread_barrier_wait (&barrier);
  printf ("Thread %d starting ...\n", data->thread_id);

  /* Perform test multiple times to increase stress */
  for (j = 0; j < 10; j++)
    {
      /* Add small random delay between iterations */
      usleep (rand() % 1000);

      printf ("Thread %d iteration %d\n", data->thread_id, j);
      test_basic_mtsafety(data);
    }

  printf ("Thread %d finished.\n", data->thread_id);
  return NULL;
}

/* Initialize thread data and create threads */
static int
create_and_run_threads (pthread_t *threads, struct thread_data *thread_data,
                       char filenames[][FILENAME_MAXLEN])
{
  int i;
  int ret;

  /* Initialize thread data */
  for (i = 0; i < NUM_THREADS; i++)
    {
      thread_data[i].thread_id = i;
      thread_data[i].success_count = 0;
      thread_data[i].error_count = 0;

      ret = pthread_create (&threads[i], NULL, thread_function, &thread_data[i]);
      if (ret != 0)
        {
          printf ("Error creating thread %d: %s\n", i, strerror (ret));
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
          printf ("Error joining thread %d: %s\n", i, strerror (ret));
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
  int total_success = 0;
  int total_errors = 0;

  for (i = 0; i < NUM_THREADS; i++)
    {
      total_success += thread_data[i].success_count;
      total_errors += thread_data[i].error_count;
    }

  printf ("Total operations: %d, Errors: %d\n",
          total_success, total_errors);

  return (total_errors ? 0 : 1);
}

static int
do_test (void)
{
  pthread_t threads[NUM_THREADS];
  struct thread_data thread_data[NUM_THREADS];
  char test_filenames[NUM_FILES][FILENAME_MAXLEN];
  int success = 1;

  /* Initialize the barrier */
  pthread_barrier_init (&barrier, NULL, NUM_THREADS);
  printf ("Test starting with %d threads\n", NUM_THREADS);

  /* Create and run threads */
  if (!create_and_run_threads (threads, thread_data, test_filenames))
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

  printf ("All threads completed\n");

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
    printf ("TEST PASSED\n");
  else
    printf ("TEST FAILED (code %d)\n", result);
  return result;
}
#else
#include <support/test-driver.c>
#endif
