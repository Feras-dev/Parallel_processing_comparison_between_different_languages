/**
 * @file multi-lookup.c
 * @author Feras Alshehri (falshehri@mail.csuchico.edu)
 * @brief main program to resolve DNS hostname using multithreading.
 * @version 0.1
 * @date 2021-04-12
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "multi-lookup.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "queue.h"
#include "util.h"

#define TRUE 1U
#define FALSE 0U
#define USLEEP 50U  // between 1 and 100, in microseconds
#define MINARGS 3
#define QUEUE_BOUND 5U
#define EMPTY_STRING ""
#define INPUTFS "%1024s"
#define MAX_INPUT_FILES 10
#define MAX_NAME_LENGTH 1025U
#define MIN_RESOLVER_THREADS 2
#define MIN_REQUESTER_THREADS 1
#define MAX_RESOLVER_THREADS 10
#define MAX_REQUESTER_THREADS MAX_INPUT_FILES
#define RESOLVER_THREADS_COUNT MAX_RESOLVER_THREADS
#define REQUESTER_THREADS_COUNT MAX_REQUESTER_THREADS
#define MAX_IP_LENGTH INET6_ADDRSTRLEN
#define USAGE "<inputFilePath> <outputFilePath>"

// internal error codes -- alter: make it an enum
#define ERROR_GENERIC -1  // unused
#define ERROR_BOGUS_HOSTNAME -2
#define ERROR_BOGUS_OUTPUT_FILE_PATH -3
#define ERROR_BOGUS_INPUT_FILE_PATH -4
#define ERROR_FAILED_TO_ENQUEUE -5
#define ERROR_INIT -6
#define ERROR_DEINIT -7
#define ERROR_THREAD_CREATION -8
#define ERROR_THREAD_JOINING -9
#define ERROR_TOO_MANY_INPUT_FILES -10

// Global static variables
static queue           myQ;
static pthread_mutex_t myQLock;
static pthread_mutex_t outputFileLock;
static FILE*           outputfp           = NULL;  //Holds the output file
static int             stillRequesting    = TRUE;
static int             numberOfInputFiles = 0;

void error_handler(int error, char* str)
{
    // All errors are recoverable and won't halt the program unless specified in the error's switch case
    int error_is_recoverable = TRUE;
    int error_code           = 0;

    switch (error) {
        case ERROR_BOGUS_HOSTNAME:
            // Bogus Hostname: Given a hostname that can not be resolved, your program
            // should output a blank string for the IP address, such that the output file
            // contains the hostname, followed by a comma, followed by a line return.
            // You should also print a message to stderr alerting the user to the bogus
            // hostname.
            fprintf(stderr, "dnslookup error: %s\n", str);
            break;

        case ERROR_BOGUS_OUTPUT_FILE_PATH:
            // Bogus Output File Path: Given a bad output file path, your program should
            // exit and print an appropriate error to stderr.
            fprintf(stderr, "Error Opening Output File: %s\n", str);
            error_is_recoverable = FALSE;
            error_code           = ENOENT;
            break;

        case ERROR_BOGUS_INPUT_FILE_PATH:
            // Bogus Input File Path: Given a bad input file path, your program should
            // print an appropriate error to stderr and move on to the next file.
            fprintf(stderr, "Error Opening Input File: %s\n", str);
            break;

        case ERROR_FAILED_TO_ENQUEUE:
            // Failed to enqueue an element into the
            fprintf(stderr, "Error failed to enqueue %s\n", str);
            break;

        case ERROR_INIT:
            // failed to initialize parameters
            fprintf(stderr, "Failed initialization\n");
            error_is_recoverable = FALSE;
            error_code           = -99;  // todo: add respective error code
            break;

        case ERROR_DEINIT:
            // failed to deinitialize parameters
            fprintf(stderr, "Failed de-initialization\n");
            error_is_recoverable = FALSE;
            error_code           = -99;  // todo: add respective error code
            break;

        case ERROR_THREAD_CREATION:
            // failed to create thread
            fprintf(stderr, "Failed to create thread\n");
            error_is_recoverable = FALSE;
            error_code           = -99;  // todo: add respective error code
            break;

        case ERROR_THREAD_JOINING:
            // failed to join thread
            fprintf(stderr, "Failed to join thread\n");
            error_is_recoverable = FALSE;
            error_code           = -99;  // todo: add respective error code
            break;

        case ERROR_TOO_MANY_INPUT_FILES:
            // failed due to too many input files
            fprintf(stderr, "Too many input files. [MAX=%d]\n", MAX_INPUT_FILES);
            error_is_recoverable = FALSE;
            error_code           = -99;  // todo: add respective error code
            break;

        default:
            break;
    }

    // if error is not recoverable, exit with error code.
    if (!error_is_recoverable) {
        exit(error_code);
    }
}

void* request(void* inputFile)
{
    char  hostname[MAX_NAME_LENGTH];  //Holds the individual hostname
    char* hostname_temp;
    FILE* inputfp = fopen((char*)inputFile, "r");

    // check input file stream
    if (!inputfp) {
        error_handler(ERROR_BOGUS_INPUT_FILE_PATH, (char*)inputFile);
        return FALSE;
    }
    printf("reading %s from thread_id %ld\n", (char*)inputFile, pthread_self());

    /* Read File and Process*/
    while (fscanf(inputfp, INPUTFS, hostname) > 0) {
        int enqueueing = TRUE;
        printf("Req> enqueuing %s\n", hostname);

        while (enqueueing) {
            // protect queue from being used by another thread
            pthread_mutex_lock(&myQLock);

            if (!queue_is_full(&myQ)) {
                // allocate space for a new node to be enqueued
                hostname_temp = malloc(MAX_NAME_LENGTH);
                strncpy(hostname_temp, hostname, MAX_NAME_LENGTH);

                if (queue_push(&myQ, hostname_temp) == QUEUE_FAILURE) {
                    error_handler(ERROR_FAILED_TO_ENQUEUE, hostname_temp);

                    // release queue to be used by another thread
                    pthread_mutex_unlock(&myQLock);

                    // failed to enqueue, free memory now or we will lose it
                    free(hostname_temp);
                }
                printf("Req> %s enqueued Successfully \n", hostname_temp);
                // release queue to be used by another thread
                pthread_mutex_unlock(&myQLock);
                enqueueing = FALSE;
            }
            else {
                // release queue to be used by another thread
                pthread_mutex_unlock(&myQLock);
                usleep(USLEEP);
            }
        }
    }
    /* Close Input File */
    if (inputfp) {
        fclose(inputfp);
        printf("Closed input file %s\n", (char*)inputFile);
    }

    /* Exit, Returning NULL*/
    return NULL;
}

void* resolve(void* outputfp)
{
    char  firstipstr[MAX_IP_LENGTH];
    char* hostname_fetched;

    while (1) {
        // protect queue from being used by another thread
        pthread_mutex_lock(&myQLock);

        if (!queue_is_empty(&myQ)) {
            hostname_fetched = (char*)queue_pop(&myQ);

            // release queue to be used by another thread
            pthread_mutex_unlock(&myQLock);

            printf("Re$> resolving %s\n", hostname_fetched);

            if (hostname_fetched) {
                /* Lookup hostname and get IP string */
                if (dnslookup(hostname_fetched, firstipstr, sizeof(firstipstr)) == UTIL_FAILURE) {
                    // can't resolve hostname. handle error, then continue.
                    error_handler(ERROR_BOGUS_HOSTNAME, hostname_fetched);

                    // set ip address to empty string to match program requirement
                    strncpy(firstipstr, EMPTY_STRING, sizeof(firstipstr));
                }
                // protect output file from being used by another thread
                pthread_mutex_lock(&outputFileLock);

                // write to output file
                fprintf((FILE*)outputfp, "%s,%s\n", hostname_fetched, firstipstr);
                fflush((FILE*)outputfp);
                printf("Re$> resolved Successfully %s,%s\n", hostname_fetched, firstipstr);

                // release output file to be used by another thread
                pthread_mutex_unlock(&outputFileLock);
            }

            // free allocated heap memory location allocated for
            // the queue node's payload we just popped
            free(hostname_fetched);
        }
        else {
            // release queue to be used by another thread
            pthread_mutex_unlock(&myQLock);
        }

        // protect queue from being used by another thread
        pthread_mutex_lock(&myQLock);

        // should we break?
        if (queue_is_empty(&myQ) && !stillRequesting) {
            // release queue to be used by another thread
            pthread_mutex_unlock(&myQLock);
            break;
        }
        // release queue to be used by another thread
        pthread_mutex_unlock(&myQLock);
    }

    /* Exit, Returning NULL*/
    return NULL;
}

int main(int argc, char* argv[])
{
    /* Check Arguments */
    if (argc < MINARGS) {
        fprintf(stderr, "Not enough arguments: %d\n", (argc - 1));
        fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
        return EXIT_FAILURE;
    }

    numberOfInputFiles = argc - 2;
    // check number of input files
    if (numberOfInputFiles > MAX_INPUT_FILES) {
        error_handler(ERROR_TOO_MANY_INPUT_FILES, EMPTY_STRING);
    }

    pthread_t reqThreads[REQUESTER_THREADS_COUNT];
    pthread_t resThreads[RESOLVER_THREADS_COUNT];

    pthread_mutex_init(&myQLock, NULL);
    pthread_mutex_init(&outputFileLock, NULL);

    // init requests queue
    if (queue_init(&myQ, QUEUE_BOUND) != QUEUE_BOUND) {
        // failed to init queue
        return FALSE;
    }

    // create requesting threads, one per input file
    for (int i = 0; i < numberOfInputFiles; i++) {
        int rc = pthread_create(&reqThreads[i], NULL, request, argv[i + 1]);
        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            error_handler(ERROR_THREAD_CREATION, EMPTY_STRING);
        }
        else {
            printf("created requesting thread #%d, for input file %s\n", i, argv[i + 1]);
        }
    }

    /* Open Output File */
    outputfp = fopen(argv[numberOfInputFiles + 1], "w");
    if (!outputfp) {
        error_handler(ERROR_BOGUS_OUTPUT_FILE_PATH, argv[numberOfInputFiles + 1]);
        return FALSE;
    }

    // Create resolver threads
    for (int i = 0; i < RESOLVER_THREADS_COUNT; ++i) {
        int rc = pthread_create(&resThreads[i], NULL, resolve, outputfp);
        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            error_handler(ERROR_THREAD_CREATION, EMPTY_STRING);
        }
        printf("created resolving thread #%d, writing to %s\n", i + 1, argv[numberOfInputFiles + 1]);
    }

    // Join on the request threads
    for (int i = 0; i < numberOfInputFiles; i++) {
        int rc = pthread_join(reqThreads[i], NULL);
        if (rc) {
            printf("ERROR; return code from pthread_join() is %d\n", rc);
            error_handler(ERROR_THREAD_JOINING, EMPTY_STRING);
        }
    }

    // toggle requesting flag to indicate production completion
    stillRequesting = FALSE;

    // Join on the resolver threads
    for (int i = 0; i < RESOLVER_THREADS_COUNT; ++i) {
        int rc = pthread_join(resThreads[i], NULL);
        if (rc) {
            printf("ERROR; return code from pthread_join() is %d\n", rc);
            error_handler(ERROR_THREAD_JOINING, EMPTY_STRING);
        }
    }

    // mutex locks clean up
    pthread_mutex_destroy(&myQLock);
    pthread_mutex_destroy(&outputFileLock);

    // free up allocated memory for queue
    queue_cleanup(&myQ);

    // Close Output File if it's open
    if (outputfp) {
        fclose(outputfp);
    }

    printf("All done! Goodbye.");

    return EXIT_SUCCESS;
}