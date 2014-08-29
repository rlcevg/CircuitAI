/*
 * Scheduler.cpp
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#include "Scheduler.h"

namespace circuit {

CQueue<CScheduler::WorkTask> CScheduler::workTasks;
std::thread CScheduler::workerThread;
std::atomic<bool> CScheduler::workerRunning(false);

CScheduler::CScheduler()
{
}

CScheduler::~CScheduler()
{
	printf("<DEBUG> Entering: %s\n", __PRETTY_FUNCTION__);
	Release();
}

void CScheduler::Release()
{
	if (workerRunning.load()) {
		workerRunning = false;
		workTasks.Push({0, nullptr, nullptr});
		if (workerThread.joinable()) {
			workerThread.join();
		}
	}
}

void CScheduler::RunTaskAt(std::shared_ptr<CTask> task, int frame)
{
	onceTasks.push_back({task, frame});
}

void CScheduler::RunTaskEvery(std::shared_ptr<CTask> task, int frameInterval)
{
	intervalTasks.push_back({task, frameInterval, -frameInterval});
}

void CScheduler::ProcessTasks(int frame)
{
	std::list<OnceTask>::iterator iter = onceTasks.begin();
	while (iter != onceTasks.end()) {
		if (iter->frame <= frame) {
			iter->task->Run();
			iter = onceTasks.erase(iter);  // alternatively, onceTasks.erase(iter++);
		} else {
			++iter;
		}
	}

	for (auto& container : intervalTasks) {
		if (frame - container.lastFrame >= container.frameInterval) {
			container.task->Run();
			container.lastFrame = frame;
		}
	}

	CQueue<FinishTask>::ProcessFunction process = [](FinishTask& item) {
		item.task->Run();
	};
	finishTasks.PopAndProcess(process);
}

void CScheduler::RunParallelTask(std::shared_ptr<CTask> task, std::shared_ptr<CTask> onSuccess)
{
	if (!workerRunning.load()) {
		workerRunning = true;
		workerThread = std::thread(&CScheduler::WorkerThread);
	}
	workTasks.Push({this, task, onSuccess});
}

void CScheduler::RemoveTask(std::shared_ptr<CTask> task)
{
	onceTasks.remove({task, 0});
	intervalTasks.remove({task, 0, 0});
}

void CScheduler::WorkerThread()
{
	WorkTask container = workTasks.Pop();
	while (workerRunning.load()) {
		container.task->Run();
		if (container.onSuccess != nullptr) {
			container.scheduler->finishTasks.Push(container);
		}
		container = workTasks.Pop();
	}
}

} // namespace circuit
