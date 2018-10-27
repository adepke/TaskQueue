/*
	Copyright (c) 2018 Andrew Depke
*/
#include "../Include/TaskQueue.h"

#include <iostream>
#include <chrono>

std::mutex PrintLock;

#define PrintSynced(x) do \
{ \
	std::lock_guard<std::mutex> Lock(PrintLock); \
	std::cout << x; \
} while (0);

// Example of a basic throwaway function.
void Work()
{
	PrintSynced("[Work] Started work on thread " << (std::this_thread::get_id()) << "\n");

	std::this_thread::sleep_for(std::chrono::seconds(5));

	PrintSynced("[Work] Finished work on thread " << std::this_thread::get_id() << "\n");
}

// Example of a minimal task object.
class SimpleTask
{
public:
	void operator()()
	{
		PrintSynced("[SimpleTask] Started work on thread " << std::this_thread::get_id() << "\n");

		std::this_thread::sleep_for(std::chrono::seconds(4));

		PrintSynced("[SimpleTask] Finished work on thread " << std::this_thread::get_id() << "\n");
	}
};

// Example of a task object that uses data.
class ComplexTask
{
public:
	int SomeData;

public:
	void operator()()
	{
		assert(SomeData == 3 && "SomeData did not match the magic value!");

		PrintSynced("[ComplexTask] Started work on thread " << std::this_thread::get_id() << "\n");

		std::this_thread::sleep_for(std::chrono::seconds(5));

		PrintSynced("[ComplexTask] Finished work on thread " << std::this_thread::get_id() << "\n");
	}
};

int main()
{
	// === Basic throwaway function demo. ===
	{
		TaskQueue<std::function<void(void)>> WorkQueue(1);  // Create a task queue that starts with 1 worker.

		WorkQueue.Enqueue(&Work);  // Place the function Work in the work queue.

		std::this_thread::sleep_for(std::chrono::seconds(5));  // Wait a bit to let the worker finish. Note: Calling Stop() before the worker finishes will not abort the task.

		WorkQueue.Stop();  // Clear the task queue and shutdown all worker threads after they have finished their active work.
	}
	// ===

	std::this_thread::sleep_for(std::chrono::seconds(3));

	// === Minimal task object demo. ===
	{
		TaskQueue<SimpleTask> WorkQueue(2);  // Create a task queue that starts with 2 workers.

		SimpleTask ExampleTask;

		WorkQueue.Enqueue(ExampleTask);   // We can queue an object by L-value or...
		WorkQueue.Enqueue(SimpleTask{});  // by R-value.

		// Note: The lifetime of ExampleTask is not a concern, assuming move semantics are properly implemented in the object.

		std::this_thread::sleep_for(std::chrono::seconds(5));

		WorkQueue.Stop();
	}
	// ===

	std::this_thread::sleep_for(std::chrono::seconds(3));

	// === Complex task object demo. ===
	{
		TaskQueue<ComplexTask> WorkQueue;  // Create a task queue that starts with 0 workers.

		ComplexTask ExampleTasks[6];

		for (int Iter = 0; Iter < 6; ++Iter)
		{
			ExampleTasks[Iter].SomeData = 3;

			WorkQueue.Enqueue(ExampleTasks[Iter]);
		}

		// Wait a bit to demonstrate that there doesn't have to be workers in a task queue for it to run. The tasks will stay queued
		// until they are either canceled or processed by a worker.
		std::this_thread::sleep_for(std::chrono::seconds(1));

		WorkQueue.Resize(3);  // Resize our worker pool to 3 workers. A task queue can be resized at any point during its lifetime.

		std::this_thread::sleep_for(std::chrono::seconds(12));

		WorkQueue.Stop();
	}
	// ===

	system("pause");

	return 0;
}