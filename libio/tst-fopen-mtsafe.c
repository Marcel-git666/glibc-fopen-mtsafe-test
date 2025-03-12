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
#define NUM_OPERATIONS 10
#define FILENAME_MAXLEN 100

/* Struct for data and threads */
struct thread_data
{
  int thread_id;
  char filename[100];
  int success_count;
  int expected_error_count;
};

static pthread_barrier_t barrier;

/* Get appropriate file opening mode string based on random selection */
static const char *
get_file_mode (int mode_choice)
{
  const char *mode;

  switch (mode_choice)
    {
      case 0: mode = "r"; break;   /* Read only */
      case 1: mode = "r+"; break;  /* Read and write, must exist */
      case 2: mode = "w"; break;   /* Write only, truncate/create */
      case 3: mode = "w+"; break;  /* Read and write, truncate/create */
      case 4: mode = "a"; break;   /* Append only, create if needed */
      case 5: mode = "a+"; break;  /* Read and append, create if needed */
      default: mode = "r"; break;
    }

  return mode;
}

/* Create test files */
static int
create_test_files (char filenames[][FILENAME_MAXLEN])
{
  FILE *file;
  int i;
  int success = 1;

  printf ("Creating test files\n");

  for (i = 0; i < NUM_FILES; i++)
    {
      file = fopen (filenames[i], "w");
      if (file == NULL)
        {
          printf ("Error creating file %s: %s\n",
                  filenames[i], strerror (errno));
          success = 0;
          continue;
        }

      fprintf (file, "Initial content for file %d\n", i);
      fclose (file);
      printf ("Created file %s\n", filenames[i]);
    }

  return success;
}

static void
cleanup_test_files (char filenames[][FILENAME_MAXLEN])
{
  FILE *file;
  char buffer[128];
  int i;

  printf ("Cleaning up test files\n");
  printf ("----------------------\n");

  for (i = 0; i < NUM_FILES; i++)
    {
      file = fopen(filenames[i], "r");
      if (file == NULL)
        {
          printf ("Error opening file %s for cleanup: %s\n",
                 filenames[i], strerror (errno));
          continue;
        }

      while (fgets (buffer, sizeof(buffer), file) != NULL)
        printf("%s", buffer);
      printf("\n");

      fclose (file);

      if (unlink (filenames[i]) == 0)
        {
          printf ("Removed file %s\n", filenames[i]);
        }
      else
        {
          printf ("Error removing file %s: %s\n",
                 filenames[i], strerror (errno));
        }
    }
}

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
      fprintf (file, "%d", data->thread_id);
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
      data->expected_error_count++;
    }
}

/* Perform reading operation if mode allows */
static void
perform_read_operation (FILE *file, const char *mode, struct thread_data *data)
{
  char buffer[128];

  /* For append mode, we need to rewind to read from the beginning */
  if (strchr (mode, 'a'))
    {
      rewind (file);
    }

  if (fgets (buffer, sizeof (buffer), file) != NULL)
    {
      printf ("Thread %d read: %s", data->thread_id, buffer);
      data->success_count++;
    }
  else
    {
      printf ("Expected error: Thread %d could not read from file\n", data->thread_id);
      data->expected_error_count++;
    }
}

/* Perform writing operation if mode allows */
static void
perform_write_operation (FILE *file, const char *mode,
                        struct thread_data *data, int iteration)
{
  int result;

  /* For write modes that truncate, add initial content back */
  if (strchr (mode, 'w'))
    {
      fprintf (file, "File rebuilt by Thread %d\n", data->thread_id);
    }

  /* For append mode or '+' mode, position at end */
  if (strchr (mode, 'a') || (strchr (mode, '+') && !strchr (mode, 'w')))
    {
      fseek (file, 0, SEEK_END);
    }

  /* Write a marker */
  result = fprintf (file, "Thread %d was here (operation %d)\n",
                   data->thread_id, iteration);

  if (result > 0)
    {
      printf ("Thread %d wrote to file\n", data->thread_id);
      data->success_count++;
    }
  else
    {
      printf ("Thread %d failed to write to file\n", data->thread_id);
      data->expected_error_count++;
    }
}

/* Function run by threads */
static void *
thread_function (void *arg)
{
  struct thread_data *data = (struct thread_data *) arg;
  FILE *file;
  int i;
  const char *mode;
  int mode_choice;

  pthread_barrier_wait (&barrier);
  printf ("Thread %d starting with file %s\n", data->thread_id, data->filename);

  test_basic_mtsafety(data);
  for (i = 0; i < NUM_OPERATIONS; i++)
    {
      /* Select random file opening mode */
      mode_choice = rand () % 6;
      mode = get_file_mode (mode_choice);

      /* Sleep before opening to create race conditions */
      usleep (rand () % 50000);

      printf ("Thread %d trying to open %s in mode '%s' (iteration %d)\n",
             data->thread_id, data->filename, mode, i);

      file = fopen (data->filename, mode);

      if (file == NULL)
        {
          printf ("Thread %d: Error opening file %s with mode %s: %s\n",
                  data->thread_id, data->filename, mode, strerror (errno));
          data->expected_error_count++;
          continue;
        }

      printf ("Thread %d successfully opened %s in mode '%s'\n",
             data->thread_id, data->filename, mode);

      /* Sleep shortly after opening to increase race chances */
      usleep (rand () % 10000);

      /* Perform operations based on file mode */
      if (strchr (mode, 'r') || strchr (mode, '+'))
        {
          perform_read_operation (file, mode, data);
        }

      if (strchr (mode, 'w') || strchr (mode, 'a') || strchr (mode, '+'))
        {
          perform_write_operation (file, mode, data, i);
        }

      /* Flush changes to disk and close file */
      fflush (file);
      fclose (file);

      printf ("Thread %d closed file %s\n", data->thread_id, data->filename);

      /* Sleep after closing to let other threads access */
      usleep (rand () % 70000);
    }

  printf ("Thread %d finished\n", data->thread_id);
  return NULL;
}

/* Initialize test files and file names */
static int
setup_test_files (char filenames[][FILENAME_MAXLEN])
{
  int i;

  /* Initialize the file names */
  for (i = 0; i < NUM_FILES; i++)
    {
      snprintf (filenames[i], FILENAME_MAXLEN, "testfile_%d.txt", i);
    }

  return create_test_files (filenames);
}

/* Initialize thread data and create threads */
static int
create_and_run_threads (pthread_t *threads, struct thread_data *thread_data,
                       char filenames[][FILENAME_MAXLEN])
{
  int i;
  int ret;

  /* Seed random number generator */
  srand (time (NULL));

  /* Initialize thread data */
  for (i = 0; i < NUM_THREADS; i++)
    {
      thread_data[i].thread_id = i;
      strcpy (thread_data[i].filename, filenames[i % NUM_FILES]);
      thread_data[i].success_count = 0;
      thread_data[i].expected_error_count = 0;

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
  int total_expected_errors = 0;

  for (i = 0; i < NUM_THREADS; i++)
    {
      total_success += thread_data[i].success_count;
      total_expected_errors += thread_data[i].expected_error_count;
    }

  printf ("Total operations: %d, Expected errors: %d\n",
          total_success + total_expected_errors, total_expected_errors);

  return 1;
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

  /* Setup test files */
  if (!setup_test_files (test_filenames))
    {
      success = 0;
      goto cleanup_barrier;
    }

  /* Create and run threads */
  if (!create_and_run_threads (threads, thread_data, test_filenames))
    {
      success = 0;
      goto cleanup_files;
    }

  /* Join threads */
  if (!join_threads (threads))
    {
      success = 0;
      goto cleanup_files;
    }

  printf ("All threads completed\n");

  /* Check and report results */
  success = report_results (thread_data);
  printf("Success report_result: %d\n", success);

cleanup_files:
  cleanup_test_files (test_filenames);

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
