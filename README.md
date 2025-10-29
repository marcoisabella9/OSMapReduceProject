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

## ** 2. Instructions **

