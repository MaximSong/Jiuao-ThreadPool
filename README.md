# 基于可变参数模板的高性能线程池(High-Performance Thread Pool With Variadic Templates)

## 项目概述

本项目实现了一个基于可变参数模板的高性能线程池，旨在优化任务提交和执行线程之间的同步机制，并提升任务调度的性能。项目支持固定数量（Fixed）和自适应（Cached）两种线程池模式，以适应不同的负载和性能需求。  
This project implements a high-performance thread pool based on variadic templates, designed to optimize synchronization between task submission and execution threads, and to enhance task scheduling performance. The project supports both fixed (Fixed) and adaptive (Cached) thread pool modes to meet various load and performance requirements.

## 项目地址

[GitHub Repository](https://github.com/MaximSong/Jiuao-ThreadPool)

## 主要功能及原理

- **生产者-消费者模型**：利用条件变量（`std::condition_variable`）和互斥锁（`std::mutex`）实现生产者-消费者模型，优化任务提交和执行线程之间的同步机制。
- **任务调度**：使用 `std::unordered_map` 和 `std::queue` 容器管理线程和任务队列，提升任务调度的性能。
- **可变参数模板**：利用可变参数模板支持接受任意函数和参数列表，实现了线程池的 `submitTask` 接口。
- **异步任务执行**：使用 `std::future` 类型定制 `submitTask` 的返回值，允许异步获取任务执行结果。
- **线程池模式**：支持固定数量（Fixed）和自适应（Cached）两种线程池模式，以适应不同的负载和性能需求。


## 使用方法
在 `main` 文件中，实现要执行的任务，并利用 `submitTask` 接口提交任务并编译即可。
```bash
g++ -std=c++11 -pthread -o main main.cpp
