# Key-Value Store Profiling using `perf`

This repository demonstrates **Linux performance profiling** of a simple **key-value store server** using the `perf` tool. The project focuses on measuring system call usage (`read`, `write`, `accept`, etc.), understanding I/O patterns, and optimizing performance as the workload scales.

---

## 📁 Repository
**GitHub:** [key-value-store-profiling-using-perf](https://github.com/Almas-zayn/key-value-store-profiling-using-perf)

---

## 🧠 Overview

The goal of this project is to understand how the **Linux kernel interacts** with user-space code during the execution of a simple key-value store server.  
We use `perf` — a powerful Linux performance analysis tool — to capture **system call frequency**, **execution time**, and **CPU utilization**.

This helps in identifying performance bottlenecks when scaling to handle a larger number of requests (e.g., 500, 1000, or 2000 random operations).

---

## ⚙️ Setup and Usage

### 1. Build the server
```bash
gcc kv-store-server.c -o kv-store-server
```

### 2. Run the server normally
```bash
./kv-store-server
```

### 3. Profile using `perf`
To trace specific system calls like `read`, `write`, and `accept`:
```bash
sudo perf stat -e syscalls:sys_enter_read,syscalls:sys_enter_write,syscalls:sys_enter_accept ./kv-store-server
```

### 4. Example Output
```text
Performance counter stats for './kv-store-server':

     27      syscalls:sys_enter_read
      3      syscalls:sys_enter_write
      1      syscalls:sys_enter_accept

  32.143884488 seconds time elapsed
   0.016479000 seconds user
   0.000000000 seconds sys
```

---

## 🧩 Perf Analysis

From the above results:

- The program executed for **~32.14 seconds**.
- The **`sys_enter_read`** syscall count (`27`) indicates moderate read activity, possibly handling client inputs or configuration reads.
- **`sys_enter_write`** count (`3`) indicates fewer writes, possibly limited to logging or response writing.
- **`sys_enter_accept` = 1** → the server accepted a single client connection.
- **User time** (`0.016s`) is very low compared to total wall time, meaning most of the time was spent waiting (likely on socket I/O).

This indicates the server is **I/O bound** rather than **CPU bound**.

---

## 🔬 Interpreting `perf stat` metrics

| Metric | Meaning | Optimization Hint |
|--------|----------|-------------------|
| `sys_enter_read` | Number of read() syscalls made | Indicates client data reads; batching may help |
| `sys_enter_write` | Number of write() syscalls made | Suggests write frequency; use buffered writes |
| `sys_enter_accept` | Number of accepted socket connections | Related to concurrency load |
| `seconds user` | CPU time spent in user-space | High = heavy computation |
| `seconds sys` | CPU time spent in kernel-space | High = frequent syscalls or context switching |
| `seconds elapsed` | Total wall-clock time | Used to calculate efficiency |

---

## 📈 Scaling Experiments

When increasing the number of operations (e.g., 500 / 1000 / 2000 random key-value requests):

| Operations | Expected Observation | Possible Cause | Suggested Optimization |
|-------------|----------------------|----------------|------------------------|
| 500 | Noticeable increase in `read` and `write` syscalls | More client-server traffic | Use batched or buffered I/O |
| 1000 | Higher CPU usage, more syscalls per second | Frequent socket reads | Implement non-blocking I/O or event loop (e.g., `epoll`) |
| 2000 | Increased latency and syscall overhead | Context switching overhead | Use thread pool or async model |

---

## 🛠️ Optimization Strategies

### 1. **Batching I/O**
Reduce the number of `read()` and `write()` calls by using buffered communication (e.g., read 4KB chunks instead of line-by-line).

### 2. **Use `epoll` instead of blocking `accept`/`read`**
`epoll` helps handle multiple clients efficiently without blocking threads.

### 3. **Thread Pool / Worker Threads**
For concurrent connections, use a fixed-size worker thread pool instead of creating threads per client.

### 4. **Memory Optimization**
- Use preallocated buffers or a slab allocator for frequent key/value objects.
- Avoid unnecessary `malloc()` / `free()` per request.

### 5. **Profiling deeper**
Use:
```bash
sudo perf record -g ./kv-store-server
sudo perf report
```
This provides a **function-level call graph** to see which functions consume the most CPU time.

---

## 📊 Further Analysis Ideas

- Compare syscall counts for **sequential vs random key access**.
- Measure latency per operation using `perf sched record`.
- Visualize `perf` results using:
  ```bash
  sudo perf report --stdio
  sudo perf script | flamegraph.pl > flamegraph.svg
  ```

---

## 🧩 Conclusion

This profiling demonstrates how `perf` can provide valuable insights into syscall-level performance.  
As the workload scales, optimizing I/O handling and reducing syscall overhead become critical.

With techniques like **epoll**, **I/O batching**, and **thread pooling**, you can significantly improve throughput and latency even under thousands of operations.

---

## 🧾 References

- [`perf` wiki](https://perf.wiki.kernel.org/)
- [`man perf-stat`](https://man7.org/linux/man-pages/man1/perf-stat.1.html)
- [Linux Performance Analysis by Brendan Gregg](http://www.brendangregg.com/perf.html)

---

> 🧑‍💻 Author: [Almas Zayn](https://github.com/Almas-zayn)
>  
> This project is part of systems performance profiling experiments.

