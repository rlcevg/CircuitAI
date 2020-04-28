/*
 * Scheduler.cpp
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#include "util/Scheduler.h"
#include "util/Utils.h"

namespace circuit {

CMultiQueue<CScheduler::WorkTask> CScheduler::workTasks;
CMultiQueue<CScheduler::WorkTask> CScheduler::pathTasks;
spring::thread CScheduler::workerThread;
spring::thread CScheduler::patherThread;
std::atomic<bool> CScheduler::workerRunning(false);
unsigned int CScheduler::counterInstance = 0;

CScheduler::CScheduler()
		: lastFrame(-1)
		, isProcessing(false)
{
	counterInstance++;
}

CScheduler::~CScheduler()
{
	counterInstance--;
	Release();
}

void CScheduler::ProcessInit()
{
	for (auto& task : initTasks) {
		task->Run();
	}
	// initTasks.clear();
}

void CScheduler::ProcessRelease()
{
	for (auto& task : releaseTasks) {
		task->Run();
	}
}

void CScheduler::Release()
{
	std::weak_ptr<CScheduler>& scheduler = self;
	CMultiQueue<WorkTask>::ConditionFunction condition = [&scheduler](WorkTask& item) -> bool {
		return !scheduler.owner_before(item.scheduler) && !item.scheduler.owner_before(scheduler);
	};
	workTasks.RemoveAllIf(condition);
	pathTasks.RemoveAllIf(condition);

	if (counterInstance == 0 && workerRunning.load()) {
		workerRunning = false;
		// At this point workTasks is empty. Push empty task in case worker stuck at Pop().
		workTasks.Push({self, nullptr, nullptr});
		if (workerThread.joinable()) {
			workerThread.join();
		}
		pathTasks.Push({self, nullptr, nullptr});
		if (patherThread.joinable()) {
			patherThread.join();
		}
	}
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
		workerThread = spring::thread(&CScheduler::WorkerThread, &workTasks);
		patherThread = spring::thread(&CScheduler::WorkerThread, &pathTasks);
	}
	workTasks.Push({self, task, onComplete});
}

void CScheduler::RunPathTask(std::shared_ptr<CGameTask> task, std::shared_ptr<CGameTask> onComplete)
{
	if (!workerRunning.load()) {
		workerRunning = true;
		workerThread = spring::thread(&CScheduler::WorkerThread, &workTasks);
		patherThread = spring::thread(&CScheduler::WorkerThread, &pathTasks);
	}
	pathTasks.Push({self, task, onComplete});
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

void CScheduler::WorkerThread(CMultiQueue<CScheduler::WorkTask>* tasks)
{
	WorkTask container = tasks->Pop();
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
		container = tasks->Pop();
	}
}

} // namespace circuit
