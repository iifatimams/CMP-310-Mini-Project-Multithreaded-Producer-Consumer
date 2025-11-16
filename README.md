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

![Running the Application](./producer_consumer%203%202%2010%20input.png)


---

## 3. Testing the Application (with Expected Outcomes)

### Test 1: Small Buffer (High Contention)

```bash
./producer_consumer 4 4 2
```

**Expected Outcome:**

- Producers frequently block on `sem_empty` due to the buffer quickly reaching full capacity.
- Consumers occasionally block on `sem_full` depending on timing gaps between production and consumption.
- Heavy synchronization causes increased contention and reduced throughput.
- Latency is relatively low because items are consumed shortly after being produced due to continuous backpressure.
- METRICS block should show:
  - Total items consumed = 4 × 20 = 80
  - Average latency in the tens of microseconds (e.g., 30–50 µs typical)
  - Throughput in the tens of thousands of items/second

---

### Test 2: Large Buffer (Low Contention)

```bash
./producer_consumer 4 4 32
```

**Expected Outcome:**

- Producers rarely block, as the buffer has ample space.
- Consumers dequeue smoothly with minimal blocking on `sem_full`.
- Producers may run ahead of consumers, allowing items to accumulate in the buffer.
- Latency increases slightly since items may remain in the buffer longer before consumption.
- METRICS block should show:
  - Total items consumed = 80
  - Average latency in the hundreds of microseconds (e.g., 500–700 µs typical)
  - Throughput comparable to or slightly higher than Test 1

---

### Test 3: Priority Handling Verification

```bash
./producer_consumer 3 2 10
```

**Expected Outcome:**

- Urgent items (marked with `(priority: urgent)`) are always consumed before any normal items.
- FIFO order is preserved within each priority class.
- Normal items are not consumed until all urgent items have been handled.
- At least 25% of items are urgent, as enforced by producer logic.
- Output will reflect urgent items being processed ahead of earlier-produced normal items.

---

### Test 4: Graceful Termination (Poison Pill)

Any valid run configuration can be used.

A correct termination must include, for every consumer:

```csharp
[Consumer-X] Finished consuming.
```

**Expected Outcome:**

- No deadlocks, hangs, or infinite waiting.
- Each consumer thread exits only after receiving exactly one poison pill.
- Final METRICS block is printed once, after all threads have completed.
- Confirms proper resource cleanup and termination logic.


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

![Full Example Output](./producer_consumer%203%202%2010.png)


---

## Author

Developed by: Fatima Mohammed S Al Sinan - g00097019
Course: CMP 310 – Operating Systems
