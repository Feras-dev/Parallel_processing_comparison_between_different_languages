package main

import (
	"bufio"                      // read from and write to files
	"fmt"                        // printing to stdout
	"github.com/gammazero/deque" // circular buffer implementation
	"net"                        // lookup hostnames's IP address
	"os"                         // open files
	"sync"                       // sync goroutines
	"time"                       // sleep
)

// global constants
var MAX_RESOLVING_GOROUTINES = 1
var QUEUE_BOUND = 1

// global variables
var myQ deque.Deque // consider a better implementation than a slice (like anything else would be better)
var myQLock = &sync.Mutex{}
var outputFileLock = &sync.Mutex{}
var doneRequesting = false

/* queue helper funcs */
func isQueueFull() bool {
	if myQ.Len() < QUEUE_BOUND {
		return false
	} else {
		return true
	}
}

func isQueueEmpty() bool {
	if myQ.Len() == 0 {
		return true
	} else {
		return false
	}
}

// producer function: reads the hostnames in inputFile and enqueue in each
// hostname into our shared "bounded" buffer
func request(requestingGoroutineGroup *sync.WaitGroup, inputFile string) {
	// to flag completion of execution of this function
	// from a calling goroutine
	defer requestingGoroutineGroup.Done()

	// open input file
	file, err := os.Open(inputFile)
	if err != nil {
		// log.Fatal(err)
		fmt.Printf("Failed to open input file [%s]\n", inputFile)
	} else {
		defer file.Close()

		// scan line by line
		scanner := bufio.NewScanner(file)
		for scanner.Scan() {
			fmt.Printf("Fetched [%s]\n", scanner.Text())
			// enqueue fetched hostname
			for true {
				myQLock.Lock()
				if !isQueueFull() {
					myQ.PushBack(scanner.Text())
					fmt.Printf("[%s] enqueued seccussfully\n", myQ.Back())
					myQLock.Unlock()
					break
				} else {
					myQLock.Unlock()
					time.Sleep(50 * time.Microsecond)
				}
			}
		}

		if err := scanner.Err(); err != nil {
			// log.Fatal(err)
			fmt.Printf("Failed while scanning input file [%s]\n", inputFile)
		}
	}

	fmt.Printf("Done with [%s]\n", inputFile)
}

func resolve(resolvingGoroutineGroup *sync.WaitGroup, outputFile string) {
	// to flag completion of execution of this function
	// from a calling goroutine
	defer resolvingGoroutineGroup.Done()

	for true {
		firstIp := ""
		myQLock.Lock()
		if isQueueEmpty() {
			myQLock.Unlock()
			if doneRequesting {
				break
			}
		} else {
			var hostname = myQ.PopFront()
			myQLock.Unlock()
			// assert hostname type
			hostname_asserted, _ := hostname.(string)
			// lookup hostname's IP address
			ip, err := net.LookupHost(hostname_asserted)
			if err == nil {
				firstIp = ip[0] // get first ip address found
			}
			// assemble line to write to output file
			lineToWrite := []byte(hostname_asserted)
			lineToWrite = append(lineToWrite, ","...)
			lineToWrite = append(lineToWrite, firstIp...)
			lineToWrite = append(lineToWrite, "\n"...)

			// write to output file
			outputFileLock.Lock()
			file, err := os.OpenFile(outputFile, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
			if err != nil {
				fmt.Printf("failed creating file: %s", err)
			}
			datawriter := bufio.NewWriter(file)
			_, _ = datawriter.WriteString(hostname_asserted + "," + firstIp + "\n")
			datawriter.Flush()
			file.Close()
			outputFileLock.Unlock()
			fmt.Printf("Resolved [%s] to [%s]\n", hostname, firstIp)
		}
	}

}

func main() {
	var requestingGoroutineGroup sync.WaitGroup
	var resolvingGoroutineGroup sync.WaitGroup

	// get command line arguments
	inputFiles := os.Args[1 : len(os.Args)-1]
	outputFile := os.Args[len(os.Args)-1]

	// remove previous output file if exists
	os.Remove(outputFile)

	// create a requesting goroutine for each input file
	for i, inputFile := range inputFiles {
		fmt.Printf("Starting requesting goroutine #%d for [%s]\n",
			i+1, inputFile)
		requestingGoroutineGroup.Add(1)
		go request(&requestingGoroutineGroup, inputFile)
	}

	// create resolving goroutines
	for i := 0; i < MAX_RESOLVING_GOROUTINES; i++ {
		fmt.Printf("Starting resolving goroutine #%d\n", i+1)
		resolvingGoroutineGroup.Add(1)
		go resolve(&resolvingGoroutineGroup, outputFile)
	}

	requestingGoroutineGroup.Wait()
	doneRequesting = true
	resolvingGoroutineGroup.Wait()

	fmt.Println("All done! goodbye!")

}
