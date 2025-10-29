# **MapReduce Parallel Computing Project**

## **1. Project Description**

This project implements two MapReduce style tasks to demostrate parallelism, 
inter-process communication (IPC) and synchronization in operating systems.

1. **Parallel Sorting (Part 1)**  
   - **Map phase:** Split a large array into chunks; each worker (thread or process) sorts its chunk in parallel using `std::sort`.  
   - **Reduce phase:** Merge the sorted chunks into one final sorted array.  
   - Implemented using both multithreading and multiprocessing modes.

2. **Max-Value Aggregation (Part 2)**  
   - **Map phase:** Each worker computes a local maximum from its assigned chunk.  
   - **Reduce phase:** Workers update a shared memory buffer that holds only one integer which is the global maximum.
   - Proper synchronization ensures safe concurrent updates to the shared memory.

### **Why Multithreading, Multiprocessing, and Synchronization?**
- **Multithreading:** Threads share the same memory space, making communication fast and efficient but requiring synchronization to avoid race conditions.  
- **Multiprocessing:** Processes have separate memory spaces, requiring explicit IPC mechanisms (like shared memory or pipes) to exchange data.  
- **Synchronization:** Ensures data consistency when multiple threads/processes access shared resources concurrently.

## **2. Instructions**

```bash
# Compile both parts
g++ -pthread parallel_sort.cpp -o parallel_sort
g++ -pthread max_aggregation.cpp -o max_aggregation -lrt

# Parallel Sorting
# Thread mode
./parallel_sort --mode thread --workers 4 --size 131072

# Process mode
./parallel_sort --mode proc --workers 8 size 131072

# Max Value Aggregation
# Thread mode
./max_aggregation --mode thread --workers 4 --size 1000000

# Process mode
./max_aggregation --mode proc --workers 8 --size 1000000
```

--mode: Between thread or proc
--workers: 1, 2, 4, 8
--size: Number of integers to process

### Output Example
```bash
Mode: thread, workers=4, siae=131072
Map time (ms): 13
Reduce time (ms): 5
Total time (ms): 18
Sorted OK: yes
Peak RSS (KB): 5376

Mode: proc, workers=8, total items=1000000
Map time (ms): 2
Final max: 2147482196
Peak RSS (KB): 7808
```

## **Structure of the Code**
### Code Overview
```bash
.
├── parallel_sort.cpp       # MapReduce-style parallel sorting
├── max_aggregation.cpp     # Max-value aggregation with shared memory
└── README.md
```
### Thread and Process Creation Diagram

Multithreading
```lua
Main Thread
   ├── Thread 1 (sort/compute local max)
   ├── Thread 2 (sort/compute local max)
   ├── Thread 3 (sort/compute local max)
   └── Thread 4 (sort/compute local max)
        ↓
     Shared Memory
        ↓
   Reducer merges results or reads global max
```
```python
Parent Process
   ├── Child Process 1
   ├── Child Process 2
   ├── Child Process 3
   └── Child Process 4
        ↓
     Shared Memory (mmap)
        ↓
   Reducer (parent) merges results or reads global max

```

### How this supports MapReduce Framework
- Map phase: Each worker independently processes its data chunk (sorting or finding local max).
- Reduce phase: Single reducer (main thread/process) aggregates all results to final output.
- MapReduce abstraction achieved within one machine, which shows parallel execution/synchronization.

## **Description of the Implementation**
### Tools and Libraries Used
POISX APIs: For Process management, shared memory, semaphores and timing.
Chrono: For precise time management.
getrusage(): To meausre peak memory usage (RSS).

### Process Management
- Multiprocessing: Implemented using fork() and wait().
- Each child process sorts or computes its local maximum independently.
- Parent (reducer) merges/reads results from shared memory after all children exit.

### IPC Mechanism
- Shared Memory: Used to share arrays or single integer between parent/child processes.
- Chosen for simplicity/speed.
- Data transfer occurs by directly writing sorted chunks or max values into shared memory region.

### Threading
- Manual thread creation: Threads are launched manually without a pool.
- Each thread handles one data chunk.
- All threads join before reduce phase.

### Synchronization Strategy
- Threads: Use atomic compare and swap for safe updates.
- Processes: Use semaphores to ensure mutual exclusion during updates to shared memory.

### Performance Measurement
- Execution time measured for
-    Map phase
-    Reduce phase
-    Total execution
- Memory use measured with getrusage().ru_maxrss.

## **5. Performance Evaluation**
### Correctness Check (Input Size = 32)
Program first verified on small input (32).
All results were correctly sorted and matched expected minimums.
No synch issues occured.

### Measured Results
<img width="920" height="544" alt="image" src="https://github.com/user-attachments/assets/dfb5c9e7-8953-4f2e-a146-c49844240dc3" />
<img width="1592" height="979" alt="image" src="https://github.com/user-attachments/assets/af3981a7-b922-4e0d-8740-e793b8ada1a3" />


### Analysis and Discussion
- Both implementations produced correct sorted arrays and max values.
- Sorting:
- Thread version (4 workers) completed in 18ms total.
- Process version (8 workers) completed in 17ms total, slightly better time because of the extra workers but higher reduce time.
- Both approaches scale efficiently but process creation and shared memory merging add overhead.
- Max Aggregation:
- Threaded version completed in 1ms while process version completed in 2ms.
- Extra ms likely comes from semphores or process scheduling.
- Memory:
- Memory use remained low across all tests.
- Multiprocessing used more due to process duplication.

## **6. Conclusion**
### Key Findings
- Multithreading provided better performance/lower overhead than multiprocessing.
- Proper synch (atomic/semaphore) was essential.
- Shared memory proved to be efficient for IPC.
### Challenges Faced
- Managing synch in process based implementation without deadlocks.
- Correclty merging sorted chunks without corrupting data.
- Measuring the performance between the threads.
### Limitations/Improvements
- Recucer runs sequentially, could be parallelized for performance.
- No fault tolerance.
