/*
	Copyright (c) 2018 Andrew Depke
*/
#pragma once

#include <mutex>
#include <condition_variable>
#include <deque>
#include <map>
#include <functional>
#include <vector>
#include <thread>
#include <cassert>
#include <algorithm>

template <typename T>
class TaskQueue
{
protected:
	std::mutex Lock;
	std::condition_variable WorkerSignal;

	std::deque<std::pair<int, T>> Queue;
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
	template <typename U>
	int Enqueue(U&& Task);

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
	bool Dequeue(T& Output);
};

template <typename T>
void TaskQueue<T>::WorkerRunnable(TaskQueue* const Manager)
{
	// Worker execution loop.
	while (Manager && !Manager->IsPendingDestroy(std::this_thread::get_id()))
	{
		// We have to lock here, otherwise if work is queued after we find that there's an empty work queue, but before we obtain a
		// lock to begin sleeping, we won't wait on the condition variable in time and will miss the queue up.
		std::unique_lock<std::mutex> EvaluationLock(Manager->Lock);

		T NewTask;

		// If we have a task, run it.
		if (Manager->Dequeue(NewTask))
		{
			EvaluationLock.unlock();

			std::invoke(NewTask);
		}

		else
		{
			// Sleep until new work is queued. Once awoken, the worker performs a state reevaluation before pulling from the work queue.
			WorkerSignal.wait(EvaluationLock);
		}
	}
}

template <typename T>
void TaskQueue<T>::DeployWorker()
{
	Workers.push_back({ std::thread(std::bind(&TaskQueue::WorkerRunnable, this, this)), false });
}

template <typename T>
void TaskQueue<T>::KillWorker()
{
	Workers.back().second = true;
	Workers.back().first.detach();

	Workers.pop_back();
}

template <typename T>
TaskQueue<T>::TaskQueue() {}

template <typename T>
TaskQueue<T>::TaskQueue(int WorkerCount)
{
	assert(WorkerCount > 0 && "WorkerCount must be greater than 0!");

	std::lock_guard<std::mutex> LocalLock(Lock);

	// Launch the workers.
	for (int Iter = 0; Iter < WorkerCount; ++Iter)
	{
		DeployWorker();
	}
}

template <typename T>
int TaskQueue<T>::GetWorkerCount()
{
	std::lock_guard<std::mutex> LocalLock(Lock);

	return Workers.size();
}

template <typename T>
void TaskQueue<T>::Resize(int NewWorkerCount)
{
	assert(NewWorkerCount > 0 && "WorkerCount must be greater than 0!");

	std::lock_guard<std::mutex> LocalLock(Lock);

	if (NewWorkerCount > static_cast<int>(Workers.size()))
	{
		for (int Iter = Workers.size(); Iter < NewWorkerCount; ++Iter)
		{
			DeployWorker();
		}
	}

	else if (NewWorkerCount < static_cast<int>(Workers.size()))
	{
		for (int Iter = Workers.size() - NewWorkerCount; Iter > 0; --Iter)
		{
			KillWorker();
		}

		// Force a reevaluation on all workers.
		WorkerSignal.notify_all();
	}
}

template <typename T>
template <typename U>  // This is used to let Enqueue abuse reference condensation, letting this function be called with either an LValue or an RValue.
int TaskQueue<T>::Enqueue(U&& Task)
{
	std::lock_guard<std::mutex> LocalLock(Lock);

	++CurrentWorkID;

	Queue.push_back({ CurrentWorkID, Task });

	WorkerSignal.notify_one();

	return CurrentWorkID;
}

template <typename T>
int TaskQueue<T>::GetUnstartedTasksCount()
{
	std::lock_guard<std::mutex> LocalLock(Lock);

	return Queue.size();
}

template <typename T>
bool TaskQueue<T>::CancelUnstartedTask(int ID)
{
	std::lock_guard<std::mutex> LocalLock(Lock);

	auto Task = std::find_if(Queue.cbegin(), Queue.cend(), [ID](const auto& Arg) { return Arg.first == ID; });
	if (Task != Queue.cend())
	{
		Queue.erase(Task);

		return true;
	}

	return false;
}

template <typename T>
void TaskQueue<T>::CancelAllUnstartedTasks()
{
	std::lock_guard<std::mutex> LocalLock(Lock);

	Queue.clear();
}

template <typename T>
void TaskQueue<T>::Stop()
{
	std::lock_guard<std::mutex> LocalLock(Lock);

	Queue.clear();

	for (auto& Worker : Workers)
	{
		Worker.second = true;
		Worker.first.detach();
	}

	// Make sure we kill off any sleeping workers.
	WorkerSignal.notify_all();
}

template <typename T>
bool TaskQueue<T>::IsPendingDestroy(std::thread::id ID)
{
	std::lock_guard<std::mutex> LocalLock(Lock);

	for (const auto& Worker : Workers)
	{
		if (Worker.first.get_id() == ID)
		{
			return Worker.second;
		}
	}

	// This could be called by an unowned worker thread, so default to true.
	return true;
}

template <typename T>
bool TaskQueue<T>::Dequeue(T& Output)
{
	if (Queue.size() > 0)
	{
		Output = Queue.front().second;
		Queue.pop_front();

		return true;
	}

	return false;
}