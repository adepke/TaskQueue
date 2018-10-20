#include "../Include/TaskQueue.h"

#include <cassert>
#include <algorithm>

#include <iostream>

void TaskQueue::WorkerRunnable(TaskQueue* const Manager)
{
	// Worker execution loop.
	while (Manager && !Manager->IsPendingDestroy(std::this_thread::get_id()))
	{
		// We have to lock here, otherwise if work is queued after we find that there's an empty work queue, but before we obtain a
		// lock to begin sleeping, we won't wait on the condition variable in time and will miss the queue up.
		std::unique_lock<std::mutex> EvaluationLock(Manager->Lock);

		auto NewTask = Manager->Dequeue();

		// We have a task, run it.
		if (NewTask)
		{
			EvaluationLock.unlock();

			std::invoke(NewTask);
		}

		// No new tasks available, sleep until we receive a signal.
		else
		{
			// Sleep until new work is queued. Once awoken, the worker performs a state reevaluation before pulling from the work queue.
			WorkerSignal.wait(EvaluationLock);
		}
	}
}

void TaskQueue::DeployWorker()
{
	Workers.push_back({ std::thread(std::bind(&TaskQueue::WorkerRunnable, this, this)), false });
}

void TaskQueue::KillWorker()
{
	Workers.back().second = true;
	Workers.back().first.detach();

	Workers.pop_back();
}

TaskQueue::TaskQueue() {}

TaskQueue::TaskQueue(int WorkerCount)
{
	assert(WorkerCount > 0 && "WorkerCount must be greater than 0!");

	std::lock_guard<std::mutex> LocalLock(Lock);

	// Launch the workers.
	for (int Iter = 0; Iter < WorkerCount; ++Iter)
	{
		DeployWorker();
	}
}

int TaskQueue::GetWorkerCount()
{
	std::lock_guard<std::mutex> LocalLock(Lock);

	return Workers.size();
}

void TaskQueue::Resize(int NewWorkerCount)
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

int TaskQueue::Enqueue(TaskType Task)
{
	std::lock_guard<std::mutex> LocalLock(Lock);

	++CurrentWorkID;

	Queue.push_back({ CurrentWorkID, Task });

	WorkerSignal.notify_one();

	return CurrentWorkID;
}

int TaskQueue::GetUnstartedTasksCount()
{
	std::lock_guard<std::mutex> LocalLock(Lock);

	return Queue.size();
}

bool TaskQueue::CancelUnstartedTask(int ID)
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

void TaskQueue::CancelAllUnstartedTasks()
{
	std::lock_guard<std::mutex> LocalLock(Lock);

	Queue.clear();
}

void TaskQueue::Stop()
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

bool TaskQueue::IsPendingDestroy(std::thread::id ID)
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

TaskQueue::TaskType TaskQueue::Dequeue()
{
	if (Queue.size() > 0)
	{
		auto Task = Queue.front();
		Queue.pop_front();

		return Task.second;
	}

	return {};
}