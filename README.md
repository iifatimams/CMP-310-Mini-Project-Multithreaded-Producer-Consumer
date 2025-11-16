# CMP-310-Mini-Project-Multithreaded-Producer-Consumer
A complete bounded-buffer Producer–Consumer application in C. The design uses blocking semaphores (empty_slots, full_slots), a mutex for mutual exclusion, and FIFO circular indexing. No busy-waiting is used; threads block correctly when buffer states change. Consumers exit gracefully via the poison-pill pattern. 
