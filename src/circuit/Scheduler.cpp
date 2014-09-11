/*
 * Scheduler.cpp
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#include "Scheduler.h"
#include "utils.h"

namespace circuit {

CMultiQueue<CScheduler::WorkTask> CScheduler::workTasks;
std::thread CScheduler::workerThread;
std::atomic<bool> CScheduler::workerRunning(false);
unsigned int CScheduler::counterInstance = 0;

CScheduler::CScheduler() :
		lastFrame(0),
		isProcessing(false)
{
	counterInstance++;
}

CScheduler::~CScheduler()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	counterInstance--;
	Release();
}

void CScheduler::Init(const std::shared_ptr<CScheduler>& thisPtr)
{
	self = thisPtr;
}

void CScheduler::Release()
{
	std::weak_ptr<CScheduler>& scheduler = self;
	CMultiQueue<WorkTask>::ConditionFunction condition = [&scheduler](WorkTask& item) -> bool {
		return !scheduler.owner_before(item.scheduler) && !item.scheduler.owner_before(scheduler);
	};
	workTasks.RemoveAllIf(condition);

	if (counterInstance == 0 && workerRunning.load()) {
		workerRunning = false;
		// At this point workTasks is empty. Push empty task in case worker stuck at Pop().
		workTasks.Push({self, nullptr, nullptr});
		if (workerThread.joinable()) {
			PRINT_DEBUG("Entering join: %s\n", __PRETTY_FUNCTION__);
			workerThread.join();
			PRINT_DEBUG("Leaving join: %s\n", __PRETTY_FUNCTION__);
		}
	}
}

void CScheduler::RunTaskAt(std::shared_ptr<CGameTask> task, int frame)
{
	onceTasks.push_back({task, frame});
}

void CScheduler::RunTaskAfter(std::shared_ptr<CGameTask> task, int frame)
{
	onceTasks.push_back({task, lastFrame + frame});
}

void CScheduler::RunTaskEvery(std::shared_ptr<CGameTask> task, int frameInterval, int frameOffset)
{
	if (frameOffset > 0) {
		RunTaskAfter(std::make_shared<CGameTask>([this, task, frameInterval]() {
			repeatTasks.push_back({task, frameInterval, lastFrame});
		}), frameOffset);
	} else {
		repeatTasks.push_back({task, frameInterval, lastFrame});
	}
}

void CScheduler::ProcessTasks(int frame)
{
	isProcessing = true;
	lastFrame = frame;

	// Process once tasks
	std::list<OnceTask>::iterator ionce = onceTasks.begin();
	while (ionce != onceTasks.end()) {
		if (ionce->frame <= frame) {
			ionce->task->Run();
			ionce = onceTasks.erase(ionce);  // alternatively, onceTasks.erase(iter++);
		} else {
			++ionce;
		}
	}

	// Process repeat tasks
	for (auto& container : repeatTasks) {
		if (frame - container.lastFrame >= container.frameInterval) {
			container.task->Run();
			container.lastFrame = frame;
		}
	}

	// Process onComplete from parallel tasks
	CMultiQueue<FinishTask>::ProcessFunction process = [](FinishTask& item) {
		item.task->Run();
	};
	finishTasks.PopAndProcess(process);

	// Update task queues
	if (!removeTasks.empty()) {
		for (auto& task : removeTasks) {
			onceTasks.remove({task, 0});
			repeatTasks.remove({task, 0, 0});
		}
		removeTasks.clear();
	}

	isProcessing = false;
}

void CScheduler::RunParallelTask(std::shared_ptr<CGameTask> task, std::shared_ptr<CGameTask> onComplete)
{
	if (!workerRunning.load()) {
		workerRunning = true;
		// TODO: Find out more about std::async, std::bind, std::future.
		workerThread = std::thread(&CScheduler::WorkerThread);
	}
	workTasks.Push({self, task, onComplete});
}

void CScheduler::RemoveTask(std::shared_ptr<CGameTask>& task)
{
	if (isProcessing) {
		removeTasks.push_back(task);
	} else {
		onceTasks.remove({task, 0});
		repeatTasks.remove({task, 0, 0});
	}
}

void CScheduler::WorkerThread()
{
	WorkTask container = workTasks.Pop();
	while (workerRunning.load()) {
		container.task->Run();
		container.task = nullptr;
		if (container.onComplete != nullptr) {
			std::shared_ptr<CScheduler> scheduler = container.scheduler.lock();
			if (scheduler) {
				scheduler->finishTasks.Push(container);
			}
			container.onComplete = nullptr;
		}
		container = workTasks.Pop();
	}
	PRINT_DEBUG("Exiting: %s\n", __PRETTY_FUNCTION__);
}

} // namespace circuit
