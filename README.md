# CMP-310 Mini Project — Multithreaded Producer–Consumer (Fall 2025)

## 1. Compilation

```bash
gcc -o producer_consumer producer_consumer.c -pthread
```

## 2. Usage

```bash
./producer_consumer <num_producers> <num_consumers> <buffer_size> [items_per_producer]
```

- `<num_producers>` — number of producer threads  
- `<num_consumers>` — number of consumer threads  
- `<buffer_size>` — size of the circular buffer  
- `[items_per_producer]` — optional (default 20)

## 3. Example Test Case

Suggested by project specification:

```bash
./producer_consumer 3 2 10
```

This runs the program with:

- 3 producers  
- 2 consumers  
- Circular buffer size = 10  
- Each producer generates 20 items (default)

## 4. Expected Output (Sample)

Output will vary because items are random:

```
[Producer-1] Produced item: 42 at index 0
[Producer-2] Produced item: 17 at index 1
[Consumer-1] Dequeued 42 from index 0
[Consumer-1] Consumed real item 42
...
[Main] Inserted POISON_PILL at index 7
[Consumer-2] Dequeued -1 from index 7
[Consumer-2] Exiting.
Summary: expected=60, consumed=60
```

## 5. Testing Notes

- The buffer never overflows or underflows due to semaphore control.
- No busy-waiting or sleeps are used.
- Consumers exit only after receiving a poison pill.
- Total consumed items always matches:
  
```
num_producers × items_per_producer
```
