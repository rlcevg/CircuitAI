/*
 * Scheduler.cpp
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#include "scheduler/Scheduler.h"
#include "util/Utils.h"
#include "util/Profiler.h"

namespace circuit {

#define MAX_JOB_THREADS		8

CMultiQueue<CScheduler::WorkTask> CScheduler::gWorkTasks;
std::vector<spring::thread> CScheduler::gWorkThreads;
int CScheduler::gMaxWorkThreads = 2;
std::atomic<bool> CScheduler::gIsWorkerRunning(false);
unsigned int CScheduler::gInstanceCount = 0;
std::atomic<int> CScheduler::gWorkerPauseId(0);
spring::mutex CScheduler::gPauseMutex;
spring::condition_variable_any CScheduler::gPauseCV;
spring::condition_variable_any CScheduler::gProceedCV;
std::size_t CScheduler::gPauseCount = 0;

CScheduler::CScheduler()
		: lastFrame(-1)
		, isProcessing(false)
{
	if (gInstanceCount == 0) {
		int maxThreads = spring::thread::hardware_concurrency();
		gMaxWorkThreads = utils::clamp(maxThreads - 1, 2, MAX_JOB_THREADS);
	}
	gInstanceCount++;
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
	gInstanceCount--;

	if (gIsWorkerRunning.load()) {
		if (gInstanceCount == 0) {
			gIsWorkerRunning = false;
			// At this point workTasks may be empty. Push dummy task in case worker stuck at Pop().
			for (unsigned int i = 0; i < gWorkThreads.size(); ++i) {
				gWorkTasks.PushBack({self, nullptr});
			}

			for (std::thread& t : gWorkThreads) {
				if (t.joinable()) {
					t.join();
				}
			}

			gWorkThreads.clear();
			gWorkTasks.Clear();

		} else {

			gPauseCount = gWorkThreads.size();
			{
				std::unique_lock<spring::mutex> lock(gPauseMutex);
				++gWorkerPauseId;
				// At this point workTasks may be empty. Push dummy task in case worker stuck at Pop().
				for (unsigned int i = 0; i < gWorkThreads.size(); ++i) {
					gWorkTasks.PushBack({self, nullptr});
				}
				gPauseCV.wait(lock, [] { return gPauseCount == 0; });
			}

			std::weak_ptr<CScheduler>& scheduler = self;
			gWorkTasks.RemoveAllIf([&scheduler](WorkTask& item) -> bool {
				return !scheduler.owner_before(item.scheduler) && !item.scheduler.owner_before(scheduler);
			});

			++gWorkerPauseId;
			gProceedCV.notify_all();
		}
	}

	finishTasks.Clear();
}

void CScheduler::StartThreads()
{
	if (gIsWorkerRunning.load()) {
		return;
	}
	gIsWorkerRunning = true;

	assert(gWorkThreads.empty());
	for (int i = 0; i < gMaxWorkThreads; ++i) {
		gWorkThreads.emplace_back(&CScheduler::WorkerThread, i);
	}
}

void CScheduler::RunJobEvery(const std::shared_ptr<IMainJob>& task, int frameInterval, int frameOffset)
{
	if (frameOffset > 0) {
		RunJobAfter(GameJob([this, task, frameInterval]() {
			repeatTasks.push_back({task, frameInterval, lastFrame});
		}), frameOffset);
	} else {
		repeatTasks.push_back({task, frameInterval, lastFrame});
	}
}

void CScheduler::ProcessJobs(int frame)
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
	gWorkTasks.PushBack({self, task});
}

void CScheduler::RunPriorityJob(const std::shared_ptr<IThreadJob>& task)
{
	gWorkTasks.PushFront({self, task});
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

void CScheduler::RemoveReleaseJob(const std::shared_ptr<IMainJob>& task)
{
	std::remove(releaseTasks.begin(), releaseTasks.end(), task);
}

void CScheduler::WorkerThread(int num)
{
#ifdef CIRCUIT_PROFILING
	tracy::SetThreadName(utils::int_to_string(num, "AI job %i").c_str());  // 15 chars max
#endif

	int workerPauseId = gWorkerPauseId;

	while (gIsWorkerRunning.load()) {

		if (workerPauseId != gWorkerPauseId) {  // parking
			std::unique_lock<spring::mutex> lock(gPauseMutex);
			int pauseId = gWorkerPauseId;
			workerPauseId = gWorkerPauseId + 1;
			if (--gPauseCount == 0) {
				gPauseCV.notify_one();
			}
			gProceedCV.wait(lock, [pauseId] { return pauseId != gWorkerPauseId; });
		}

		TracyPlot("Jobs", (int64_t)gWorkTasks.Size());
		WorkTask container = gWorkTasks.Pop();

		std::shared_ptr<CScheduler> scheduler = container.scheduler.lock();
		if (scheduler == nullptr) {
			continue;
		}

		if (scheduler->isRunning.load()) {

			std::shared_ptr<IMainJob> onComplete = container.task->Run(num);
			if (onComplete != nullptr) {
				scheduler->finishTasks.PushBack(onComplete);
			}

		}
	}
}

} // namespace circuit
