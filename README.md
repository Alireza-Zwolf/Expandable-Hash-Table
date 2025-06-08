# Expandable Hash Table (Concurrent Probing)

This project implements a set of **concurrent hash tables using probing**, with a focus on an **Expandable Hash Table** that dynamically grows as needed during runtime.

It was developed as part of **CS 798: Multicore Programming** at the University of Waterloo, and explores different synchronization techniques for designing scalable concurrent data structures.

---

## 🚀 Features

- ✅ Lock-based hash table with linear probing
- ✅ Lock-free probing hash table
- ✅ Dynamically **expandable hash table** (`alg_d.h`) with resizing
- ✅ Safe concurrent operations (`insert`, `erase`, `contains`)
- ✅ Fine-grained atomic operations using `std::atomic` and memory ordering
- ✅ Thread-safe benchmarking for performance evaluation

---

## 📁 File Overview

| File             | Description |
|------------------|-------------|
| `alg_d.h`        | 📌 **Main implementation** — expandable concurrent hash table using dynamic resizing |
| `alg_a.h`        |  Lock-based static hash table |
| `alg_b.h`        |  Lock-free static hash table |
| `benchmark.cpp`  | Benchmarking tool to test hash table implementations under multi-threaded load |

---

## ⚙️ How Dynamic Expansion Works

The expandable hash table implemented in `alg_d.h` uses a cooperative and thread-safe mechanism to increase capacity at runtime when the table becomes too full.

- A new, larger table is created when the load factor exceeds a preset threshold (e.g., 0.5).
- All threads participate in **migrating keys** from the old table to the new one.
- Atomic **compare-and-swap (CAS)** operations are used to initiate the new table safely.
- Expansion avoids infinite recursion by ensuring helping threads do not trigger further expansions during migration.
- Old table memory can optionally be reclaimed when it is no longer in use (e.g., failed CAS).
- Expansion size is typically 4× the number of keys, but smaller or same-size expansions are allowed under special conditions.

---

## 🧪 Benchmarking

Use the `benchmark.out` tool to test and compare performance of different hash table algorithms (A–D).

### Example Usage

```bash
make benchmark
./benchmark.out -a D -sT 1000 -sR 1000000 -m 5000 -t 8
```

Key Flags:
-a : Algorithm (A, B, C, or D)

-sT: Initial table size threshold

-sR: Key range

-m : Run time in milliseconds

-t : Number of threads


## 📈 Evaluation

![benchmark_results_plot](https://github.com/user-attachments/assets/bbb6936c-c24d-4502-9d96-72c1cb23fb76)

Here is a benchmark result of running static (algorithm a-c) and expandable (algorithm d) hash tables under high workload, where the expandable hash table can support heavy workload up to ~65 million operations on 16 threads ( and ~80 millions on 31 threads).

## Credits
The benchmark, util, and make files are provided by Dr. [Trevor Brown](https://www.cs.utoronto.ca/~tabrown/).
