/**
 * @file multi-lookup.c
 * @author Feras Alshehri (falshehri@mail.csuchico.edu)
 * @brief main program to resolve DNS hostname using multi-processing.
 * @version 0.1
 * @date 2021-05-17
 * 
 * @copyright Copyright (c) 2021
 */

#include "multi-lookup.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util.h"

#define TRUE 1U
#define FALSE 0U
#define USLEEP 200U  // between 1 and 100, in microseconds
#define MINARGS 3
#define QUEUE_BOUND 5
#define EMPTY_STRING ""
#define INPUTFS "%1024s"
#define MAX_INPUT_FILES 10
#define MAX_NAME_LENGTH 1025
#define MIN_RESOLVER_THREADS 2
#define MIN_REQUESTER_THREADS 1
#define MAX_RESOLVER_THREADS 10
#define MAX_REQUESTER_THREADS MAX_INPUT_FILES
#define RESOLVER_PROCESSES_COUNT MAX_RESOLVER_THREADS
#define REQUESTER_PROCESSES_COUNT MAX_REQUESTER_THREADS
#define MAX_IP_LENGTH INET6_ADDRSTRLEN
#define USAGE "<inputFilePath> <outputFilePath>"
#define NO_PAYLOAD EMPTY_STRING

// internal error codes -- alter: make it an enum
#define ERROR_GENERIC -1  // unused
#define ERROR_BOGUS_HOSTNAME -2
#define ERROR_BOGUS_OUTPUT_FILE_PATH -3
#define ERROR_BOGUS_INPUT_FILE_PATH -4
#define ERROR_FAILED_TO_ENQUEUE -5
#define ERROR_INIT -6
#define ERROR_DEINIT -7
#define ERROR_PROCESS_CREATION -8
#define ERROR_PROCESS_JOINING -9
#define ERROR_TOO_MANY_INPUT_FILES -10

// Global static variables
static char*               myQ;
static pthread_mutex_t*    myQLock;
static pthread_mutex_t*    outputFileLock;
static pthread_mutexattr_t myQLockAttr;
static pthread_mutexattr_t outputFileLockAttr;
static pthread_cond_t*     myQIsFull;
static pthread_condattr_t  myQIsFullAttr;
static char*               child_pid_str;
static char                popped_element[MAX_NAME_LENGTH];
static int                 resolving_pids[RESOLVER_PROCESSES_COUNT];
static int                 requesting_pids[REQUESTER_PROCESSES_COUNT];
static FILE*               outputfp = NULL;  //Holds the output file
static int*                stillRequesting;
static int                 numberOfInputFiles = 0;

/* utility functions */
int get_process_num_from_PID(int pid)
{
    for (int i = 0; i < RESOLVER_PROCESSES_COUNT; i++) {
        if (resolving_pids[i] == pid) {
            return i + 1;
        }
    }

    for (int i = 0; i < REQUESTER_PROCESSES_COUNT; i++) {
        if (requesting_pids[i] == pid) {
            return i + 1;
        }
    }

    return 99;
}

char* qGet(int n)
{
    return myQ + (n * MAX_NAME_LENGTH);
}

void printBuffContent(char* x)
{
    printf("%s", x);
    printf(" Queue content:");
    for (int i = 0; i < QUEUE_BOUND; i++) {
        printf("%s, ", qGet(i));
    }
    printf(" from [P%d]\n", get_process_num_from_PID(getpid()));
}

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

        case ERROR_PROCESS_CREATION:
            // failed to create process
            fprintf(stderr, "Failed to create process\n");
            error_is_recoverable = FALSE;
            error_code           = -99;  // todo: add respective error code
            break;

        case ERROR_PROCESS_JOINING:
            // failed to join process
            fprintf(stderr, "Failed to join process\n");
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

/* queue management functions */
int queue_init()
{
    // allocate myQ in shared memory
    myQ = (char*)mmap(NULL,
                      (sizeof(char*) * QUEUE_BOUND * MAX_NAME_LENGTH),
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANON,
                      -1,
                      0);
    if (myQ == MAP_FAILED) {
        return FALSE;
    }

    // ADD INVALID PAYLOAD IN ALL ELEMENTS
    for (int i = 0; i < QUEUE_BOUND; i++) {
        strcpy(&myQ[i], NO_PAYLOAD);
        // confirm
        if (strcmp(&myQ[i], NO_PAYLOAD)) {
            return FALSE;
        }
    }
    return TRUE;
}

int queue_is_full()
{
    // scan all elements in my queue and see if it has any empty payload
    for (int i = 0; i < QUEUE_BOUND; i++) {
        if (!strcmp(qGet(i), NO_PAYLOAD)) {
            return FALSE;
        }
    }
    return TRUE;
}

int queue_is_empty()
{
    // scan all elements in my queue and see if it has any non-empty payload
    for (int i = 0; i < QUEUE_BOUND; i++) {
        if (strcmp(qGet(i), NO_PAYLOAD) != 0) {
            return FALSE;
        }
    }
    return TRUE;
}

int queue_push(char* payload)
{
    // add payload at the end of my queue
    for (int i = 0; i < QUEUE_BOUND; i++) {
        // find the first element in queue with empty element
        if (!strcmp(qGet(i), NO_PAYLOAD)) {
            strcpy(qGet(i), payload);

            // verify write
            if (!strcmp(qGet(i), payload)) {
                return TRUE;
            }
            else {
                return FALSE;
            }
        }
    }
    return FALSE;
}

char* queue_pop()
{
    char temp_prev[MAX_NAME_LENGTH];
    char temp_curr[MAX_NAME_LENGTH];

    // pop the first element, and shift all other elements
    // towards the front
    strcpy(popped_element, qGet(0));

    strcpy(temp_prev, qGet(QUEUE_BOUND));
    strcpy(qGet(QUEUE_BOUND), NO_PAYLOAD);

    for (int i = QUEUE_BOUND - 1; i >= 0; i--) {
        strcpy(temp_curr, qGet(i));
        strcpy(qGet(i), temp_prev);
        strcpy(temp_prev, temp_curr);
    }

    return popped_element;
}

/* producer and consumer functions */
void request(char* inputFile)
{
    char  hostname[MAX_NAME_LENGTH];  //Holds the individual hostname
    FILE* inputfp = fopen(inputFile, "r");

    // check input file stream
    if (!inputfp) {
        error_handler(ERROR_BOGUS_INPUT_FILE_PATH, inputFile);
        return;
    }
    printf("Req> reading %s from P%d\n",
           inputFile,
           get_process_num_from_PID(getpid()));

    /* Read File and Process*/
    while (fscanf(inputfp, INPUTFS, hostname) > 0) {
        printf("Req> enqueuing %s\n", hostname);

        // protect queue from being used by another thread
        pthread_mutex_lock(myQLock);

        while (queue_is_full()) {
            // queue is full, sleep current process until signaled to wake up, and
            // release the specified mutex lock (i.e. myQLock)
            pthread_cond_wait(myQIsFull, myQLock);
        }
        // queue is not full, enqueue hostname at hand
        if (queue_push(hostname) == FALSE) {
            error_handler(ERROR_FAILED_TO_ENQUEUE, hostname);

            // release queue to be used by another thread
            pthread_mutex_unlock(myQLock);
        }
        printf("Req> %s enqueued Successfully [P%d] \n",
               hostname,
               get_process_num_from_PID(getpid()));

        // release queue to be used by another thread
        pthread_mutex_unlock(myQLock);

        // printBuffContent("Req>");
    }

    /* Close Input File */
    if (inputfp) {
        fclose(inputfp);
        printf("Req> Closed input file %s\n", (char*)inputFile);
    }

    while (wait(NULL) > 0)
        ;  // wait for child processes to finish

    if (getpid() == requesting_pids[0]) {
        // requesting parent is done. no more requesting.
        printf("Req> Done requesting!\n");
        *stillRequesting = FALSE;
    }
    exit(0);
}

void resolve()
{
    char  firstipstr[MAX_IP_LENGTH];
    char* hostname_fetched;

    while (1) {
        // lock queue
        pthread_mutex_lock(myQLock);

        if (getpid() == resolving_pids[0]) {
            // only parent resolving process checks and signals for requesting process
            // to avoide signaling too many requesting processes at once
            if (!queue_is_full()) {
                // signal that we have now an empty spot in the queue
                pthread_cond_signal(myQIsFull);
            }
        }

        if (!queue_is_empty()) {
            hostname_fetched = queue_pop();
            // printf("Res> [P%d] fetched %s from queue\n", get_process_num_from_PID(getpid()), hostname_fetched);

            // release queue to be used by another process
            pthread_mutex_unlock(myQLock);

            printf("Res> resolving %s\n", hostname_fetched);

            if (hostname_fetched) {
                /* Lookup hostname and get all IPs found */
                if (dnslookup(hostname_fetched, firstipstr, sizeof(firstipstr)) == UTIL_FAILURE) {
                    // can't resolve hostname. handle error, then continue.
                    error_handler(ERROR_BOGUS_HOSTNAME, hostname_fetched);

                    // set ip address to empty string to match program requirement
                    strncpy(firstipstr, EMPTY_STRING, sizeof(firstipstr));
                }

                // protect output file from being used by another process
                pthread_mutex_lock(outputFileLock);

                // write to output file
                fprintf((FILE*)outputfp, "%s,%s\n", hostname_fetched, firstipstr);
                fflush((FILE*)outputfp);

                // release output file to be used by another process
                pthread_mutex_unlock(outputFileLock);

                printf("Res> [%s] resolved Successfully to [%s]\n", hostname_fetched, firstipstr);
            }
        }
        else {
            // release queue to be used by another process
            pthread_mutex_unlock(myQLock);
        }

        // check if we should be terminating out of this loop
        // we break if the queue is empty AND we are done requesting
        pthread_mutex_lock(myQLock);
        if (queue_is_empty() && *stillRequesting == FALSE) {
            pthread_mutex_unlock(myQLock);
            // terminate if queue is empty AND we are done requesting
            break;
        }
        else {
            pthread_mutex_unlock(myQLock);
        }
    }

    while (wait(NULL) > 0)
        ;  // wait for child processes to finish

    if (getpid() == resolving_pids[0]) {
        // requesting parent is done. no more requesting.
        printf("Res> Done resolving!\n");
        return;
    }
    exit(0);
}

/* Entry point of the program */
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

    // map synchronization mutexes, CVs, and any other common variables in shared memory
    // to allow all processes to access them
    myQLock         = (pthread_mutex_t*)mmap(NULL,
                                     sizeof(*myQLock),
                                     PROT_READ | PROT_WRITE,
                                     MAP_SHARED | MAP_ANON,
                                     -1,
                                     0);
    outputFileLock  = (pthread_mutex_t*)mmap(NULL,
                                            sizeof(*outputFileLock),
                                            PROT_READ | PROT_WRITE,
                                            MAP_SHARED | MAP_ANON,
                                            -1,
                                            0);
    myQIsFull       = (pthread_cond_t*)mmap(NULL,
                                      sizeof(*myQIsFull),
                                      PROT_READ | PROT_WRITE,
                                      MAP_SHARED | MAP_ANON,
                                      -1,
                                      0);
    stillRequesting = (int*)mmap(NULL,
                                 sizeof(*stillRequesting),
                                 PROT_READ | PROT_WRITE,
                                 MAP_SHARED | MAP_ANON,
                                 -1,
                                 0);
    outputfp        = (FILE*)mmap(NULL,
                           sizeof(*outputfp),
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED | MAP_ANON,
                           -1,
                           0);

    // init mutexes attributes
    pthread_mutexattr_init(&myQLockAttr);
    pthread_mutexattr_setpshared(&myQLockAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_init(&outputFileLockAttr);
    pthread_mutexattr_setpshared(&outputFileLockAttr, PTHREAD_PROCESS_SHARED);

    // init mutex locks
    pthread_mutex_init(myQLock, &myQLockAttr);
    pthread_mutex_init(outputFileLock, &outputFileLockAttr);

    // init conditional variables attributes
    pthread_condattr_init(&myQIsFullAttr);
    pthread_condattr_setpshared(&myQIsFullAttr, PTHREAD_PROCESS_SHARED);

    // init conditional variables
    pthread_cond_init(myQIsFull, &myQIsFullAttr);

    // init requests queue
    if (!queue_init()) {
        // failed to init queue
        error_handler(ERROR_INIT, EMPTY_STRING);
        return FALSE;
    }

    // set shared variables
    *stillRequesting = TRUE;

    // printBuffContent("main>");

    /* Open Output File */
    outputfp = fopen(argv[numberOfInputFiles + 1], "w");
    if (!outputfp) {
        error_handler(ERROR_BOGUS_OUTPUT_FILE_PATH, argv[numberOfInputFiles + 1]);
        return FALSE;
    }

    // creating requesting and resolving processes
    int child_pid = fork();
    switch (child_pid) {
        case -1:
            // failed forking
            sprintf(child_pid_str, "%d", child_pid);
            error_handler(ERROR_PROCESS_CREATION, child_pid_str);
            break;

        case 0:

            // append parent requester process ID
            requesting_pids[0] = getpid();
            printf("main> created requesting process #1 [%d]\n", requesting_pids[0]);

            // create remaining child requester processes
            for (int i = 1; i < numberOfInputFiles; i++) {
                int temp_pid = fork();
                if (temp_pid < 0) {
                    // error
                    char* child_pid_str = NULL;
                    sprintf(child_pid_str, "%d", child_pid);
                    error_handler(ERROR_PROCESS_CREATION, child_pid_str);
                }
                else if (temp_pid == 0) {
                    // last created child process. Append and run.
                    requesting_pids[i] = getpid();
                    printf("main> created requesting process #%d [%d]\n", i + 1, requesting_pids[i]);
                    request(argv[i]);
                    wait(NULL);
                }
                else {
                    // parent. Create next child
                    continue;
                }
            }
            request(argv[numberOfInputFiles]);
            wait(NULL);
            break;

        default:
            // append parent resolver process ID
            resolving_pids[0] = getpid();
            printf("main> created resolving process #1 [%d]\n", resolving_pids[0]);

            // create remaining child resolver processes
            for (int i = 1; i < RESOLVER_PROCESSES_COUNT; i++) {
                int temp_pid = fork();
                if (temp_pid < 0) {
                    // error
                    sprintf(child_pid_str, "%d", child_pid);
                    error_handler(ERROR_PROCESS_CREATION, child_pid_str);
                }
                else if (temp_pid == 0) {
                    // last created child process. Append and run.
                    resolving_pids[i] = getpid();
                    printf("main> created resolving process #%d [%d]\n", i + 1, resolving_pids[i]);
                    resolve();
                    break;
                }
                else {
                    // parent. Create next child
                    continue;
                }
            }

            // parent completed creating child processes. run.
            resolve();
            break;
    }

    // printf("main> cleaning up");

    // mutex locks clean up
    pthread_mutex_destroy(myQLock);
    pthread_mutex_destroy(outputFileLock);

    // printf(".");

    // mutex attributes clean up
    pthread_mutexattr_destroy(&myQLockAttr);
    pthread_mutexattr_destroy(&outputFileLockAttr);

    // printf(".");

    // conditional variables clean up
    pthread_cond_destroy(myQIsFull);

    // printf(".");

    // CV attributes clean up
    pthread_condattr_destroy(&myQIsFullAttr);

    // printf(".");

    // Close Output File if it's open
    if (outputfp) {
        fclose(outputfp);
    }
    printf("Done!\n");
    // printBuffContent("main>");
    printf("main> All done! Goodbye.");// -yours truly, pid:%d\n", get_process_num_from_PID(getpid()));

    return EXIT_SUCCESS;
}