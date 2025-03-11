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
#define NUM_THREADS 16

/* Struct for data and threads */
struct thread_data
{
  int thread_id;
  char filename[100];
  int success_count;
  int error_count;
  pthread_barrier_t *barrier;
};

/* Create test files */
static void
create_test_files (char filenames[][100])
{
  FILE *file;

  printf ("Creating test files\n");

  for (int i = 0; i < NUM_FILES; i++)
    {
      file = fopen (filenames[i], "w");
      if (file == NULL)
        {
          printf ("Error creating file %s: %s\n",
                  filenames[i], strerror (errno));
          continue;
        }

      fprintf (file, "Initial content for file %d\n", i);
      fclose (file);
      printf ("Created file %s\n", filenames[i]);
    }
}

/* Clean up test files */
static void
cleanup_test_files (char filenames[][100])
{
  printf ("Cleaning up test files\n");

  for (int i = 0; i < NUM_FILES; i++)
    {
      if (unlink (filenames[i]) == 0)
        {
          printf ("Removed file %s\n", filenames[i]);
        }
    }
}

/* Function run by threads */
static void *
thread_function (void *arg)
{
  struct thread_data *data = (struct thread_data *) arg;

  pthread_barrier_wait (data->barrier);
  printf ("Thread %d starting\n", data->thread_id);

  /* Implementace testovací logiky */
  /* ... */

  printf ("Thread %d finished\n", data->thread_id);
  return NULL;
}

static int
do_test (void)
{
  pthread_t threads[NUM_THREADS];
  struct thread_data thread_data[NUM_THREADS];
  int ret;
  char test_filenames[NUM_FILES][100];
  pthread_barrier_t start_barrier;

  // Initialize the barrier
  pthread_barrier_init (&start_barrier, NULL, NUM_THREADS);
  printf ("Test starting with %d threads\n", NUM_THREADS);
 /* Initialize the file names */
  for (int i = 0; i < NUM_FILES; i++)
    {
    snprintf (test_filenames[i], 100, "testfile_%d.txt", i);
  }

  create_test_files (test_filenames);
  /* Initialize thread data and create threads */
  for (int i = 0; i < NUM_THREADS; i++)
    {
      thread_data[i].thread_id = i;
      strcpy (thread_data[i].filename, test_filenames[i % NUM_FILES]);
      thread_data[i].success_count = 0;
      thread_data[i].error_count = 0;

      ret = pthread_create (&threads[i], NULL, thread_function, &thread_data[i]);
      if (ret != 0)
        {
          printf ("Error creating thread %d: %s\n", i, strerror (ret));
          return 1;
        }
    }

  /* Wait for all threads to complete */
  for (int i = 0; i < NUM_THREADS; i++)
    {
      ret = pthread_join (threads[i], NULL);
      if (ret != 0)
        {
          printf ("Error joining thread %d: %s\n", i, strerror (ret));
          return 1;
        }
    }

  printf ("All threads completed\n");

  /* Check results */
  int total_success = 0;
  int total_errors = 0;

  for (int i = 0; i < NUM_THREADS; i++)
    {
      total_success += thread_data[i].success_count;
      total_errors += thread_data[i].error_count;
    }

    cleanup_test_files (test_filenames);
  printf ("Total operations: %d, Errors: %d\n",
          total_success + total_errors, total_errors);

  return (total_errors == 0) ? 0 : 1; /* 0 means success */
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
