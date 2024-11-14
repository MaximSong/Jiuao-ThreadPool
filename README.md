# High-Performance Thread Pool With Variadic Templates

## Project Overview  
This project implements a high-performance thread pool based on variadic templates, designed to optimize synchronization between task submission and execution threads, and to enhance task scheduling performance. The project supports both fixed (Fixed) and adaptive (Cached) thread pool modes to meet various load and performance requirements.

## Project Repository

[GitHub Repository](https://github.com/MaximSong/Jiuao-ThreadPool)

## Key Features

- **Producer-Consumer Model**：Uses `std::condition_variable` and `std::mutex` to implement a producer-consumer model.
- **Task Scheduling:**：Manages threads and task queues using `std::unordered_map` and `std::queue containers`.
- **Variadic Templates**：Utilizes `variadic templates` to accept functions with arbitrary parameter lists.
- **Asynchronous Task Execution**：Uses `std::future` to customize the return value of submitTask, enabling asynchronous retrieval of task execution results.
- **Thread Pool Modes**：Supports both `fixed-size (Fixed)` and `adaptive (Cached)` thread pool modes to accommodate varying load and performance requirements.


## Usage
In the `main` file, implement the tasks to be executed, submit them using the submitTask interface, and compile as follows:
```bash
g++ -std=c++11 -pthread -o main main.cpp
