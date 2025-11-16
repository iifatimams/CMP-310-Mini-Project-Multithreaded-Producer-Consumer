# CMP-310 Mini Project – Producer–Consumer (Fall 2025)

## 1. Introduction
This project implements the classic Producer–Consumer problem using C, POSIX threads, semaphores, and a mutex. The goal is to manage concurrent access to a shared bounded buffer while preventing race conditions, ensuring correct blocking behavior, and supporting clean thread termination.

---

## 2. System Design

### 2.1 Circular Buffer
A fixed-size FIFO buffer stores produced items. Two indices (`in` and `out`) implement circular wrapping.

### 2.2 Synchronization
- `empty_slots`: counts available buffer positions  
- `full_slots`: counts items ready for consumption  
- `mutex`: ensures exclusive access to the buffer  

These ensure correct blocking without busy-waiting.

### 2.3 Poison-Pill Termination
After all producers finish, the main thread inserts one `POISON_PILL` per consumer. Consumers terminate once they dequeue it.

---

## 3. Implementation Summary
- Each producer generates items and inserts them into the buffer.
- Each consumer removes items and processes them.
- The system prevents data corruption using mutual exclusion.
- Semaphores ensure that producers wait when the buffer is full and consumers wait when it is empty.
- The program supports configurable numbers of producers, consumers, buffer size, and items per producer.

---

## 4. Conclusion
The project demonstrates practical use of multithreading, synchronization primitives, and bounded-buffer coordination. All requirements of the CMP-310 mini project were met, including blocking semantics, poison-pill shutdown, and safe buffer access.
