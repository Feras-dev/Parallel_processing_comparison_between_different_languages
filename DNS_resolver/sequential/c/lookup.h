/**
 * @file lookup.h
 * @author Feras Alshehri (falshehri@mail.csuchico.edu)
 * @brief header file.
 * @version 0.1
 * @date 2021-04-12
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef LOOKUP_H
#define LOOKUP_H

/**
 * @brief function to fetch each hostname in an input file,
 *          and enqueue it into our FIFO queue.
 * 
 * @param inputFile pointer to input file of type FILE*.
 * @return void* returns NULL upon complete execution.
 */
void* request(void* inputFile);

/**
 * @brief function to resolve hostnames stores in FIFO queue
 *          and write each IP address to the output file.
 * 
 * @param outputfp pointer to output file of type FILE*.
 * @return void* returns NULL upon complete execution.
 */
void* resolve(void* outputfp);

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

/**
 * @brief main function.
 * 
 * @param argc number of command line arguments.
 * @param _argv array of char*s command line arguments, 
 *              each node contains an argument. 
 * @return int 1 upon completion.
 */
int main(int argc, char* _argv[]);

#endif /* LOOKUP_H */