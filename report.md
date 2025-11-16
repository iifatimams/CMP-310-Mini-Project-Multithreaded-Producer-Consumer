# CMP-310 Mini Project - Producer-Consumer Report
Fall 2025 — POSIX Threads, Semaphores, and Circular Buffer

## 1. Introduction
This project implements a multithreaded Producer–Consumer system using a bounded circular buffer. The goal is to coordinate multiple producers and consumers using semaphores and a mutex while preventing race conditions and ensuring correct blocking behavior. The assignment also requires a graceful termination strategy using poison pills.

---

## 2. Design Decisions

### 2.1 Circular Bounded Buffer
A circular queue was chosen because it provides a simple FIFO structure that reuses buffer space efficiently. Two indices (`in` and `out`) track insertion and removal. The modulo operation ensures correct wrap-around.

### 2.2 Synchronization Mechanisms
The program uses three key synchronization primitives:

- **Mutex (`pthread_mutex_t`)**  
  Protects access to shared buffer indices and data. Only one thread can modify the buffer at a time, preventing race conditions.

- **Semaphore `empty_slots`**  
  Tracks how many slots are free.  
  Producers block automatically when the buffer is full.

- **Semaphore `full_slots`**  
  Tracks how many slots contain valid items.  
  Consumers block when the buffer is empty.

This design ensures that the system *never* busy-waits and relies entirely on kernel-managed blocking.

### 2.3 Poison Pill Termination
After all producers finish generating items, the main thread inserts exactly one `POISON_PILL` for each consumer. A consumer that dequeues this value terminates immediately.  
This avoids deadlocks and ensures each consumer stops cleanly even if producers finish earlier.

### 2.4 Command-Line Configuration
Producers, consumers, and buffer size are provided through command-line arguments. This enables dynamic testing under varying concurrency loads.

---

## 3. Challenges and Solutions

### Challenge 1: Avoiding Race Conditions
Multiple threads access the buffer simultaneously.  
**Solution:** All access to `buffer`, `in`, and `out` is protected by a single mutex, ensuring atomic modifications.

### Challenge 2: Preventing Buffer Overflow/Underflow
Incorrect synchronization can allow producers to write into a full buffer or consumers to read from an empty one.  
**Solution:** Semaphores strictly enforce resource limits:
- `empty_slots` prevents overflow  
- `full_slots` prevents underflow  

### Challenge 3: Graceful Thread Termination
Without a termination signal, consumers could block indefinitely.  
**Solution:** One poison pill per consumer ensures predictable termination without race conditions.

### Challenge 4: Handling Mixed Timing Between Threads
Depending on system scheduling, producers or consumers may run faster.  
**Solution:** Blocking semaphores naturally balance both sides without sleep delays or busy-waiting.

---

## 4. Conclusion
The completed system satisfies all project requirements: it uses a circular bounded buffer, POSIX threads, semaphores, a mutex, input validation, and poison-pill termination. Producers and consumers synchronize correctly, avoid race conditions, and block efficiently without busy-waiting. The design is robust, modular, and suitable for demonstrating core operating system synchronization concepts.
