/*
 * Scheduler.cpp
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#include "scheduler/Scheduler.h"
#include "util/Utils.h"

namespace circuit {

#define MAX_JOB_THREADS		8

CMultiQueue<CScheduler::WorkTask> CScheduler::workTasks;
std::vector<spring::thread> CScheduler::workThreads;
int CScheduler::maxWorkThreads = 1;
std::atomic<bool> CScheduler::workerRunning(false);
unsigned int CScheduler::counterInstance = 0;

CScheduler::CScheduler()
		: lastFrame(-1)
		, isProcessing(false)
		, numWorkProcess(0)
{
	if (counterInstance == 0) {
		int maxThreads = spring::thread::hardware_concurrency();
		maxWorkThreads = utils::clamp(maxThreads - 1, 2, MAX_JOB_THREADS);
	}
	counterInstance++;
	isRunning = true;
}

CScheduler::~CScheduler()
{
	Release();
}

void CScheduler::ProcessInit()
{
	for (auto& task : initTasks) {
		task->Run();
	}
//	initTasks.clear();
	StartThreads();
}

void CScheduler::ProcessRelease()
{
	for (auto& task : releaseTasks) {
		task->Run();
	}

	Release();
}

void CScheduler::Release()
{
	if (!isRunning) {
		return;
	}
	isRunning = false;
	counterInstance--;

	std::weak_ptr<CScheduler>& scheduler = self;
	workTasks.RemoveAllIf([&scheduler](WorkTask& item) -> bool {
		return !scheduler.owner_before(item.scheduler) && !item.scheduler.owner_before(scheduler);
	});

	if (counterInstance == 0 && workerRunning.load()) {
		workerRunning = false;
		// At this point workTasks is empty. Push empty task in case worker stuck at Pop().
		for (unsigned int i = 0; i < workThreads.size(); ++i) {
			workTasks.PushBack({self, nullptr});
		}
		for (std::thread& t : workThreads) {
			if (t.joinable()) {
				t.join();
			}
		}
		workThreads.clear();
		workTasks.Clear();
	}

	barrier.Wait([this]() { return numWorkProcess == 0; });

	finishTasks.Clear();
}

void CScheduler::StartThreads()
{
	if (workerRunning.load()) {
		return;
	}
	workerRunning = true;

	assert(workThreads.empty());
	for (int i = 0; i < maxWorkThreads; ++i) {
		std::thread t = spring::thread(&CScheduler::WorkerThread, i);
		workThreads.push_back(std::move(t));
	}
}

void CScheduler::RunTaskEvery(const std::shared_ptr<IMainJob>& task, int frameInterval, int frameOffset)
{
	if (frameOffset > 0) {
		RunTaskAfter(GameJob([this, task, frameInterval]() {
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
	finishTasks.PopAndProcessAll(process);  // one heavy / lite ???

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

void CScheduler::RunParallelJob(const std::shared_ptr<IThreadJob>& task)
{
	workTasks.PushBack({self, task});
}

void CScheduler::RunPriorityJob(const std::shared_ptr<IThreadJob>& task)
{
	workTasks.PushFront({self, task});
}

void CScheduler::RemoveJob(const std::shared_ptr<IMainJob>& task)
{
	if (isProcessing) {
		removeTasks.push_back(task);
	} else {
		onceTasks.remove({task, 0});
		repeatTasks.remove({task, 0, 0});
	}
}

void CScheduler::WorkerThread(int num)
{
	while (workerRunning.load()) {
		WorkTask container = workTasks.Pop();

		std::shared_ptr<CScheduler> scheduler = container.scheduler.lock();
		if (scheduler == nullptr) {
			continue;
		}

		scheduler->barrier.NotifyOne([scheduler]() { scheduler->numWorkProcess++; });
		if (scheduler->isRunning) {

			std::shared_ptr<IMainJob> onComplete = container.task->Run(num);
			if (onComplete != nullptr) {
				scheduler->finishTasks.PushBack(onComplete);
			}

		}
		scheduler->barrier.NotifyOne([scheduler]() { scheduler->numWorkProcess--; });
	}
}

} // namespace circuit
