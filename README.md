# CMP-310 Mini Project — Producer–Consumer (Fall 2025)

This repository contains a multithreaded **Producer–Consumer** implementation written in **C** using POSIX threads. The project uses a **circular bounded buffer**, **blocking semaphores**, and a **mutex** to ensure safe synchronization between multiple producers and consumers. Consumers terminate cleanly using a **poison-pill mechanism**.

---

## Features
- Circular bounded buffer with FIFO behavior  
- Multiple producers and consumers (configurable through command-line arguments)  
- Synchronization using:
  - `sem_t empty_slots`
  - `sem_t full_slots`
  - `pthread_mutex_t buffer_mutex`
- No busy-waiting — all blocking is handled using semaphores  
- Graceful consumer termination using `POISON_PILL`  
- Input validation and error-handling  

---

## Compilation

```bash
gcc -o producer_consumer producer_consumer.c -pthread
```

---

## Usage

```bash
./producer_consumer <num_producers> <num_consumers> <buffer_size> [items_per_producer]
```

Example (recommended test case):

```bash
./producer_consumer 3 2 10
```

---

## Summary of Design
- A shared structure holds:
  - the buffer
  - indices (`in`, `out`)
  - semaphores
  - mutex
  - counters for produced/consumed items
- Producers:
  - wait on `empty_slots`
  - lock the buffer
  - insert an item
  - unlock
  - signal `full_slots`
- Consumers:
  - wait on `full_slots`
  - lock the buffer
  - dequeue an item
  - unlock
  - signal `empty_slots`
  - exit if item = `POISON_PILL`

---

## Example Output
```
[Producer-1] Produced item: 42 at index 0
[Consumer-1] Dequeued value 42 from index 0
[Consumer-1] Consumed real item: 42
...
[Main] Inserted POISON_PILL at index 9
[Consumer-2] Received POISON_PILL. Exiting.
Summary: expected real items = 60, consumed = 60
```

---

## Files
- `producer_consumer.c` — main C source file  
- `report.md` — project explanation (required for submission)  
- `.gitignore` — ignores build artifacts  

---

## Course
CMP-310 — Operating Systems Mini Project (Fall 2025), American University of Sharjah
