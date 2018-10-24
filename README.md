# TaskQueue
### Modern C++ Generic Thread Pool

Compact yet powerful concurrent thread pool implementation. Requires C++17.

### Documentation
*Functions marked with `[Thread Safe]` indicate safe concurrent usage.*

#### Class
```cpp
TaskQueue<T>
```
Creates a task queue object, T is the type of task that can be queued. T must be a functor.

#### Constructors

```cpp
TaskQueue::TaskQueue()
```
Default constructor, does not deploy any workers.

```cpp
TaskQueue::TaskQueue(int WorkerCount)
```
Allocates `WorkerCount` number of threads, which perform a state evaluation and immediately sleep if no work is in the queue.

#### Functions

```cpp
int TaskQueue::GetWorkerCount()
```
`[Thread Safe]` Returns the number of active workers. This includes sleeping workers, but does not include workers recently killed that are finishing their active task.

```cpp
void TaskQueue::Resize(int NewWorkerCount)
```
`[Thread Safe]` Resizes the worker pool to have `NewWorkerCount` worker threads, either allocating and deploying new workers or killing off some workers, depending on `NewWorkerCount`. If a worker that is performing a task is killed, they will complete their task before deallocating.

```cpp
template <typename U>
int Enqueue(U&& Task)
```
`[Thread Safe]` Enqueues a new task for a worker to process when available. If no workers are in the pool, the task will sit in queue until a resize. U must match the type T. This extra template is used to leverage reference condensation, letting `Task` be an L value or an R value. Returns the ID of the newly enqueued task, `-1` if a failure occurred.

```cpp
int GetUnstartedTasksCount()
```
`[Thread Safe]` Returns the number of tasks in the queue that haven't been started by a worker yet.

```cpp
bool CancelUnstartedTask(int ID)
```
`[Thread Safe]` Cancels a task if it hasn't yet been started by a worker. `ID` is the ID of the target task, which can be obtained by the return value of Enqueue. Returns if the task was successfully cancelled or not.

```cpp
void CancelAllUnstartedTasks()
```
`[Thread Safe]` Cancels all tasks that haven't yet been started by workers.

```cpp
void Join()
```
`[Thread Safe]` Blocks the calling thread until the workers have run all tasks in the queue and have synchronized. Prevents additional work from being enqueued from other threads until this call has completed. Once completed, the task queue is left in an empty working state with no workers.

```cpp
void Stop()
```
`[Thread Safe]` Clears the work queue (Cancels unstarted tasks) and marks all workers for destruction. Kills any and all sleeping workers. Used to cleanly shutdown a task queue. Once completed, the task queue is left in an empty working state with no workers.
