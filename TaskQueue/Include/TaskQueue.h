#pragma once

#include <mutex>
#include <condition_variable>
#include <deque>
#include <map>
#include <functional>
#include <vector>
#include <thread>
#include <atomic>

class TaskQueue
{
	typedef std::function<void(void)> TaskType;

protected:
	std::mutex Lock;
	std::condition_variable WorkerSignal;

	std::deque<std::pair<int, TaskType>> Queue;
	int CurrentWorkID;

	std::vector<std::pair<std::thread, bool>> Workers;

	// [Not Locked] Launches a worker thread.
	void DeployWorker();

	// [Not Locked] Kills off a worker thread.
	void KillWorker();

public:
	TaskQueue();
	TaskQueue(int WorkerCount);

	int GetWorkerCount();

	// Change the size of the worker pool.
	void Resize(int NewWorkerCount);

	// Queue up a new task. Returns the task ID.
	int Enqueue(TaskType Task);

	// Returns how many tasks are in the queue waiting to start.
	int GetUnstartedTasksCount();

	// Cancel a task in queue that hasn't started yet. Returns whether or not the operation was successful.
	bool CancelUnstartedTask(int ID);

	// Cancel all remaining tasks that haven't started.
	void CancelAllUnstartedTasks();

	// Clears the work queue and kills workers after they finish their current task.
	void Stop();

// Worker Tools
protected:
	void WorkerRunnable(TaskQueue* const Manager);

	bool IsPendingDestroy(std::thread::id ID);

	// [Not Locked]
	TaskType Dequeue();
};