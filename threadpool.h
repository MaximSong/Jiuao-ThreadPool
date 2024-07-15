#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <thread>
#include <future>
const int TASK_MAX_THRESHOlD = INT32_MAX;
const int THREAD_MAX_THRESHOLD = 1024;
const int Thread_idle_time = 60;

enum class PoolMode
{
    MODE_FIXED,
    MODE_CACHED,
};
// thread
class Thread
{
public:
    using ThreadFunc = std::function<void(int)>;
    Thread(ThreadFunc func)
        : func_(func),
          threadId_(generateId_++)
    {
    }
    ~Thread() = default;
    void start()
    {
        std::thread t(func_, threadId_);
        t.detach(); // t and func_ are running concurrently
    }
    int getId() const
    {
        return threadId_;
    }

private:
    ThreadFunc func_;
    static int generateId_;
    int threadId_;
};
// threadpool
int Thread::generateId_ = 0;
class ThreadPool
{
public:
    ThreadPool()
        : initThreadSize_(0),
          taskSize_(0),
          idleThreadNum_(0),
          curThreadSize_(0),
          taskQueMaxThreshold_(TASK_MAX_THRESHOlD),
          threadMaxThreshold_(THREAD_MAX_THRESHOLD),
          poolMode_(PoolMode::MODE_FIXED), ispoolRunning_(false)
    {
    }
    ~ThreadPool()
    {
        ispoolRunning_ = false;
        std::unique_lock<std::mutex> lock(taskQueMtx_);
        notEmpty_.notify_all();
        exitCond_.wait(lock, [&]() -> bool
                       { return threads_.size() == 0; });
    }
    void start(int initThreadSize = std::thread::hardware_concurrency())
    {
        ispoolRunning_ = true;
        initThreadSize_ = initThreadSize;
        curThreadSize_ = initThreadSize;
        for (int i = 0; i < initThreadSize_; i++)
        {
            auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
            int threadId = ptr->getId();
            threads_.emplace(threadId, std::move(ptr));
            // threads_.emplace_back(std::move(ptr));
        }
        // start thread
        for (int i = 0; i < initThreadSize_; i++)
        {
            threads_[i]->start();
            idleThreadNum_++;
        }
    }
    void setMode(PoolMode mode)
    {
        if (checkRunningStatus())
        {
            return;
        }
        poolMode_ = mode;
    }
    // set task queue max threshold
    void setTaskQueMaxThreshold(int threshold)
    {
        if (checkRunningStatus())
        {
            return;
        }
        taskQueMaxThreshold_ = threshold;
    }
    // set thread max threshold in cached mode
    void setThreadMaxThreshold(int threshold)
    {
        if (checkRunningStatus())
        {
            return;
        }
        if (poolMode_ == PoolMode::MODE_CACHED)
        {
            threadMaxThreshold_ = threshold;
        }
    }
    // submit task to thread pool
    // 使用可变参数模板,实现对任意参数的传递
    template <typename Fuc, typename... Args>
    auto submitTask(Fuc &&func, Args &&...args) -> std::future<decltype(func(args...))>
    {
        using Rtype = decltype(func(std::forward<Args>(args)...));
        auto task = std::make_shared<std::packaged_task<Rtype()>>(std::bind(std::forward<Fuc>(func), std::forward<Args>(args)...));
        // auto task = std::make_shared<std::packaged_task<Rtype()>>(std::bind(std::forward<Fuc>(func), std::forward<Args>(args)...));
        std::future<Rtype> result = task->get_future();

        std::unique_lock<std::mutex> lock(taskQueMtx_);
        // thread comunicate and wait
        if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() -> bool
                               { return taskQueue_.size() < (size_t)taskQueMaxThreshold_; }))
        {
            std::cerr << "task queue is full , submit task fail" << std::endl;
            auto task = std::make_shared<std::packaged_task<Rtype()>>(
                []() -> Rtype
                { return Rtype(); });
            (*task)();
            return task->get_future();
        }
        // using Task = std::function<void()>;
        taskQueue_.emplace([task]()
                           { (*task)(); });
        taskSize_++;
        // notify not empty
        notEmpty_.notify_all();
        // cached mode and task size is bigger than thread size, cache mode is more efficient in small and short time task
        if (poolMode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadNum_ && curThreadSize_ < threadMaxThreshold_)
        {
            std::cout << "create new thread tid:" << std::this_thread::get_id() << std::endl;
            auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
            int threadId = ptr->getId();
            threads_.emplace(threadId, std::move(ptr));
            // threads_.emplace_back(std::move(ptr));
            threads_[threadId]->start();
            curThreadSize_++;
            idleThreadNum_++;
        }
        return result;
    }
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    void threadFunc(int threadid)
    {
        // std::cout<<"begin threadFunc tid:"<<std::this_thread::get_id()<<std::endl;
        // std::cout<<"end threadFunc tid:"<<std::this_thread::get_id()<<std::endl;
        auto lastTime = std::chrono::high_resolution_clock().now(); // thread function init time

        for (;;)
        {
            Task task;
            {
                std::unique_lock<std::mutex> lock(taskQueMtx_);
                std::cout << "try to get task tid:" << std::this_thread::get_id() << std::endl;
                // recycle thread
                while (taskQueue_.size() == 0)
                {
                    if (!ispoolRunning_)
                    {
                        threads_.erase(threadid);
                        std::cout << "thread " << std::this_thread::get_id() << " exit" << std::endl;
                        exitCond_.notify_all();
                        return;
                    }
                    if (poolMode_ == PoolMode::MODE_CACHED)
                    {
                        // time out 1s
                        if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
                        {
                            auto now = std::chrono::high_resolution_clock().now();
                            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                            if (duration.count() >= Thread_idle_time && curThreadSize_ > initThreadSize_)
                            {
                                threads_.erase(threadid);
                                curThreadSize_--;
                                idleThreadNum_--;
                                std::cout << "thread " << std::this_thread::get_id() << " exit" << std::endl;
                                return;
                            }
                        }
                    }
                    else
                    {
                        notEmpty_.wait(lock);
                    }
                    // if (!ispoolRunning_) {
                    //     threads_.erase(threadid);
                    //     std::cout<<"thread "<<std::this_thread::get_id()<<" exit"<<std::endl;
                    //     exitCond_.notify_all();
                    //     return;
                    // }
                }
                idleThreadNum_--;
                std::cout << "get task tid:" << std::this_thread::get_id() << std::endl;
                task = taskQueue_.front();
                taskQueue_.pop();
                taskSize_--;
                if (taskQueue_.size() > 0)
                {
                    notEmpty_.notify_all();
                }
                notFull_.notify_all();
            }
            if (task != nullptr)
            {
                task();
            }
            idleThreadNum_++;
            lastTime = std::chrono::high_resolution_clock().now(); // update the thread function done time
        }
    }
    // check pool running or not
    bool checkRunningStatus()
    {
        return ispoolRunning_;
    }

private:
    // std:: vector<std::unique_ptr<Thread>> threads_; //thread list
    std::unordered_map<int, std::unique_ptr<Thread>> threads_;
    int initThreadSize_;            // init thread size
    std::atomic_int curThreadSize_; // current thread size
    std::atomic_int idleThreadNum_; // idle thread number
    int threadMaxThreshold_;        // thread max threshold

    // task任务=》线程池的任务
    using Task = std::function<void()>;
    std::queue<Task> taskQueue_; // task queue
    std::atomic_int taskSize_;   // task size
    int taskQueMaxThreshold_;    // task queue max threshold

    std::mutex taskQueMtx_;            // task queue mutex
    std::condition_variable notFull_;  // task queue not full
    std::condition_variable notEmpty_; // task queue not empty
    std::condition_variable exitCond_; // exit condition

    PoolMode poolMode_;              // pool mode right now
    std::atomic_bool ispoolRunning_; // thread pool is running or not
};
#endif // THREADPOOL_H