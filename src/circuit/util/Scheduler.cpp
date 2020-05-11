/*
 * Scheduler.cpp
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#include "util/Scheduler.h"
#include "util/Utils.h"

namespace circuit {

#define MAX_JOB_THREADS		4

CMultiQueue<CScheduler::WorkTask> CScheduler::workTasks;
CMultiQueue<CScheduler::PathTask> CScheduler::pathTasks;
spring::thread CScheduler::workerThread;
std::vector<spring::thread> CScheduler::patherThreads;
int CScheduler::maxPathThreads = 1;
std::atomic<bool> CScheduler::workerRunning(false);
unsigned int CScheduler::counterInstance = 0;

CScheduler::CScheduler()
		: lastFrame(-1)
		, isProcessing(false)
		, isWorkProcess(false)
		, numPathProcess(0)
{
	if (counterInstance == 0) {
		int maxThreads = spring::thread::hardware_concurrency();
		maxPathThreads = utils::clamp(maxThreads - 2, 1, MAX_JOB_THREADS);
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
	// initTasks.clear();
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
	pathTasks.RemoveAllIf([&scheduler](PathTask& item) -> bool {
		return !scheduler.owner_before(item.scheduler) && !item.scheduler.owner_before(scheduler);
	});

	if (counterInstance == 0 && workerRunning.load()) {
		workerRunning = false;
		// At this point workTasks is empty. Push empty task in case worker stuck at Pop().
		workTasks.Push({self, nullptr, nullptr});
		if (workerThread.joinable()) {
			workerThread.join();
		}
		workTasks.Clear();

		for (unsigned int i = 0; i < patherThreads.size(); ++i) {
			pathTasks.Push({self, nullptr, nullptr, nullptr});
		}
		for (std::thread& t : patherThreads) {
			if (t.joinable()) {
				t.join();
			}
		}
		patherThreads.clear();
		pathTasks.Clear();
	}

	barrier.Wait([this]() { return !isWorkProcess && (numPathProcess == 0); });

	finishTasks.Clear();
	pathedTasks.Clear();
}

void CScheduler::StartThreads()
{
	if (workerRunning.load()) {
		return;
	}
	workerRunning = true;

	workerThread = spring::thread(&CScheduler::WorkerThread);

	assert(patherThreads.empty());
	for (int i = 0; i < maxPathThreads; ++i) {
		std::thread t = spring::thread(&CScheduler::PatherThread, i);
		patherThreads.push_back(std::move(t));
	}
}

void CScheduler::RunTaskEvery(const std::shared_ptr<CGameTask>& task, int frameInterval, int frameOffset)
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
	if (!finishTasks.PopAndProcess(process)) {  // one heavy
		pathedTasks.PopAndProcessAll([](PathedTask& item) {  // many lite
			std::shared_ptr<IPathQuery> query = item.query.lock();
			if (query != nullptr) {
				item.onComplete(query);
			}
		});
	}

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

void CScheduler::RunParallelTask(const std::shared_ptr<CGameTask>& task, const std::shared_ptr<CGameTask>& onComplete)
{
	StartThreads();
	workTasks.Push({self, task, onComplete});
}

void CScheduler::RunPathTask(const std::shared_ptr<IPathQuery>& query, PathFunc&& task, PathedFunc&& onComplete)
{
	StartThreads();
	pathTasks.Push({self, query, std::move(task), std::move(onComplete)});
}

void CScheduler::RemoveTask(const std::shared_ptr<CGameTask>& task)
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
	while (workerRunning.load()) {
		WorkTask container = workTasks.Pop();

		std::shared_ptr<CScheduler> scheduler = container.scheduler.lock();
		if (scheduler != nullptr) {
			scheduler->barrier.NotifyOne([scheduler]() { scheduler->isWorkProcess = true; });
			if (scheduler->isRunning) {

				container.task->Run();
				if (container.onComplete != nullptr) {
					scheduler->finishTasks.Push(container);
				}

			}
			scheduler->barrier.NotifyOne([scheduler]() { scheduler->isWorkProcess = false; });
		}
	}
}

void CScheduler::PatherThread(int num)
{
	while (workerRunning.load()) {
		PathTask container = pathTasks.Pop();

		std::shared_ptr<IPathQuery> query = container.query.lock();
		if (query == nullptr) {
			continue;
		}
		std::shared_ptr<CScheduler> scheduler = container.scheduler.lock();
		if (scheduler == nullptr) {
			continue;
		}

		scheduler->barrier.NotifyOne([scheduler]() { scheduler->numPathProcess++; });
		if (scheduler->isRunning) {

			container.task(query, num);
			if (container.onComplete != nullptr) {
				scheduler->pathedTasks.Push(container);
			}

		}
		scheduler->barrier.NotifyOne([scheduler]() { scheduler->numPathProcess--; });
	}
}

} // namespace circuit
