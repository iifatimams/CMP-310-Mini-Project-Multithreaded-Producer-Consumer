# CMP-310 Mini Project — Multithreaded Producer–Consumer (Fall 2025)

This README provides instructions on how to compile, run, and test the multithreaded Producer–Consumer application. It includes complete usage examples, expected outcomes, and a full sample output.

---

## 1. Compilation

Compile the program using:

```bash
gcc -o producer_consumer producer_consumer.c -pthread
```

This will generate the executable:

```bash
./producer_consumer
```

---

## 2. Running the Application

The program requires three command-line arguments:

```bash
./producer_consumer <num_producers> <num_consumers> <buffer_size>
```

### Example:

```bash
./producer_consumer 3 2 10
```

- 3 producer threads  
- 2 consumer threads  
- Buffer size = 10  
- Each producer generates 20 items  
- Total real items = 3 × 20 = 60

![Running the Application](./3.png)

---

## 3. Testing the Application (with Expected Outcomes)

### Test 1: Small Buffer (High Contention)

```bash
./producer_consumer 4 4 2
```

**Expected Outcome:**

- Producers will frequently block on `sem_empty` because the buffer is always full.
- Consumers will frequently block on `sem_full` during timing gaps.
- Throughput will be lower because of heavy synchronization.
- Average latency will be higher due to longer waiting times.
- METRICS block should show:
  - Higher latency (hundreds–thousands of microseconds)
  - Lower throughput (compared to large buffer test)
  - Total items consumed = 4 × 20 = 80

---

### Test 2: Large Buffer (Low Contention)

```bash
./producer_consumer 4 4 32
```

**Expected Outcome:**

- Producers rarely block since the buffer has enough space.
- Consumers dequeue smoothly with minimal waiting.
- Throughput will be significantly higher than Test 1.
- Latency will be lower because items are not stuck waiting in queues.
- METRICS block should show:
  - Lower latency (tens–hundreds of microseconds)
  - Total items consumed = 80

---

### Test 3: Priority Handling Verification

```bash
./producer_consumer 3 2 10
```

**Expected Outcome:**

- Lines containing `(priority: urgent)` will appear in consumer output before any pending normal priority items.
- FIFO order is preserved inside each priority queue.
- Normal items are not consumed while any urgent items remain.
- Total urgent item percentage should be at least 25%.

---

### Test 4: Graceful Termination (Poison Pill)

Any run configuration can be used.  
A correct termination must include:

```csharp
[Consumer-X] Finished consuming.
```

for each consumer thread.

**Expected Outcome:**

- No hanging, deadlocks, or infinite waiting.
- Each consumer terminates only after receiving one poison pill.
- Final METRICS block must appear once at the end.

---

## 4. Full Example (Input + Output)

### Command:

```bash
./producer_consumer 3 2 10
```

### Representative Output (Truncated):

```csharp
[Producer-1] Produced item: 225 (priority: normal)
[Producer-3] Produced item: 507 (priority: urgent)
[Consumer-2] Consumed item: 225 (priority: normal)
[Consumer-1] Consumed item: 507 (priority: urgent)
...
[Producer-1] Finished producing 20 items.
[Producer-2] Finished producing 20 items.
[Producer-3] Finished producing 20 items.
...
[Consumer-2] Finished consuming.
[Consumer-1] Finished consuming.

========= METRICS =========
Total items consumed: 60  
Average latency: 267.40 microseconds  
Throughput: 28182.25 items/second  
Runtime: 0.002 seconds
```

This confirms correct synchronization, priority handling, poison-pill shutdown, and metric computation.

![Full Example Output](./33.png)

---

## License

This project is part of CMP 310 coursework. Educational use only.

---

## Author

Developed by: [Your Name]  
Course: CMP 310 – Operating Systems
