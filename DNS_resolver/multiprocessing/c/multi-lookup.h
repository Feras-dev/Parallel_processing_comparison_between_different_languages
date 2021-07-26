/**
 * @file multi-lookup.h
 * @author Feras Alshehri (falshehri@mail.csuchico.edu)
 * @brief header file for multiprocessing DNS resolver.
 * @version 0.1
 * @date 2021-04-12
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H
/*********************/
/* Utility functions */
/*********************/
/**
 * @brief Get the process number from based on its PID.
 * 
 * @param pid process ID to convert to the process number.
 * @return int process number.
 */
int get_process_num_from_PID(int pid);

/**
 * @brief get an element from the queue without removing it from
 *          the queue.
 * 
 * @param n the element index, where 0 is the first element.
 * @return char* the string stored in that element.
 */
char* qGet(int n);

/**
 * @brief print the content opf the bounded buffer to stdout.
 * 
 * @param x a string to include before printing the content on
 *          the same line. Pass an empty string for none.
 */
void printBuffContent(char* x);

/**
 * @brief function to handle errors.
 * 
 * @param error error code, internally defined as 
 *              preprocessor directives in multi-lookup.c.
 * @param str  Any supplemental string for the error to pass 
 *             on to the user as feedback. 
 * @return void* returns NULL upon complete execution.
 */
void error_handler(int error, char* str);

/******************************/
/* queue management functions */
/******************************/

/**
 * @brief initialize the bounded buffer (queue) to empty strings.
 * 
 * @return int returns 1 if successful. 0 otherwise.
 */
int queue_init();

/**
 * @brief check if the queue is empty (all elements are empty strings).
 * 
 * @return int 1 if queue is empty. 0 otherwise.
 */
int queue_is_full();

/**
 * @brief check if the queue is full (no empty string elements).
 * 
 * @return int 1 if queue is full. 0 otherwise.
 */
int queue_is_full();

/**
 * @brief push a new element to the queue.
 * 
 * @param payload the payload to be enqueued.
 * @return int 1 upon successful push to the queue. 0 otherwise.
 */
int queue_push(char* payload);

/**
 * @brief pop the front element of the queue, and shift all
 *          elements towards the front fo the queue.
 * 
 * @return char* the element popped from the front of the queue.
 */
char* queue_pop();

/*******************************/
/* producer and consumer funcs */
/*******************************/
/**
 * @brief function to fetch each hostname in an input file,
 *          and enqueue it into our FIFO queue.
 * 
 * @param inputFile pointer to input file of type FILE*.
 * @return void* returns NULL upon complete execution.
 */
void request();

/**
 * @brief function to resolve hostnames stores in FIFO queue
 *          and write each IP address to the output file.
 * 
 * @param outputfp pointer to output file of type FILE*.
 * @return void* returns NULL upon complete execution.
 */
void resolve();

/**
 * @brief main function.
 * 
 * @param argc number of command line arguments.
 * @param _argv array of char*s command line arguments, 
 *              each node contains an argument. 
 * @return int 1 upon completion.
 */
int main(int argc, char* _argv[]);

#endif /* MULTI_LOOKUP_H */