
import queue
import sys
import os
import socket
import threading
import time

# global DONE_REQUESTING
QUEUE_BOUND = 1
RESOLVER_THREAD_COUNT = 1
# global myQ  # bounded buffer

DONE_REQUESTING = False
myQ = queue.Queue(QUEUE_BOUND)  # bounded buffer

outputFile = sys.argv[-1]
inputFiles = sys.argv[1:-1]

def get_ip_by_hostname(hostname):
    '''
    Function to get IP addresses of hostname
    '''
    try:
        rc = socket.getaddrinfo(hostname,0)
        for r in rc:
            if r[0] is socket.AddressFamily.AF_INET:  # ipv4
                return r[4][0]  #ipv4 address
        return "UNHANDLED" 
    except:
        pass
        return "UNHANDLED" 


def request(inputFile):
    '''
    Producer function.
    '''
    with open(inputFile, "r") as _if:
        for line in _if:
            enqueueing = True
            while(enqueueing):
                if not myQ.full():
                    myQ.put(line)
                    print(f"Req> enqueued [{line[:-1]}] successfully")
                    enqueueing = False
                else:
                    # resolution is not guaranteed -- and that's OK
                    time.sleep(5e-5)
    return


def resolve(outputFileLock):
    '''
    Consumer function.
    '''
    while True:
        with open(outputFile, "a+") as of:
            if not myQ.empty():
                hostname = myQ.get()
                hostname_striped = hostname.rstrip("\n")
                ip = get_ip_by_hostname(hostname_striped)
                # atomic write to output file operation
                outputFileLock.acquire(blocking=True)
                print(f"{hostname_striped},{ip}\n", end="", file=of)
                outputFileLock.release()
                print(f"Res> Resolved [{hostname_striped}] to [{ip}]")
            elif DONE_REQUESTING and myQ.empty():
                return


if __name__ == '__main__':
    reqThreads = []
    resThreads = [] 

    outputFileLock = threading.Lock()

    # remove output file if exits previously
    os.remove(outputFile)

    for i in range(len(inputFiles)):
        reqThreads.append(threading.Thread(target=request, args=(inputFiles[i],)))
        reqThreads[i].start()
        print(f"Created requesting thread for input {inputFiles[i]}")

    for j in range(RESOLVER_THREAD_COUNT):
        resThreads.append(threading.Thread(target=resolve, args=(outputFileLock,)))
        resThreads[j].start()
        print(f"Created resolving thread #{j}")

    for k in reqThreads:
        k.join()
    
    print("Done requesting!")
    DONE_REQUESTING = True

    for l in resThreads:
        l.join()

    print("All done! goodbye.")
    

    
