/*
 * Scheduler.h
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include "MultiQueue.h"
#include "GameTask.h"

#include <list>
#include <memory>
#include <thread>
#include <atomic>

namespace circuit {

class CScheduler {
public:
	CScheduler();
	virtual ~CScheduler();

	void Init(const std::shared_ptr<CScheduler>& thisPtr);

private:
	void Release();

public:
	/*
	 * Add task at specified frame, or execute immediately at next frame
	 */
	void RunTaskAt(std::shared_ptr<CGameTask> task, int frame = 0);

	/*
	 * Add task at frame relative to current frame
	 */
	void RunTaskAfter(std::shared_ptr<CGameTask> task, int frame = 0);

	/*
	 * Add task at specified interval
	 */
	void RunTaskEvery(std::shared_ptr<CGameTask> task, int frameInterval = 30, int frameOffset = 0);

	/*
	 * Process queued tasks at specified frame
	 */
	void ProcessTasks(int frame);

	/*
	 * Run concurrent task, finalize on success at main thread
	 */
	void RunParallelTask(std::shared_ptr<CGameTask> task, std::shared_ptr<CGameTask> onSuccess = nullptr);

	/*
	 * Remove scheduled task from queue
	 */
	void RemoveTask(std::shared_ptr<CGameTask>& task);

private:
	std::weak_ptr<CScheduler> self;
	int lastFrame;
	bool isProcessing;

	struct BaseContainer {
		BaseContainer(std::shared_ptr<CGameTask> task) :
			task(task) {}
		std::shared_ptr<CGameTask> task;
		bool operator==(const BaseContainer& other) const {
			return task == other.task;
		}
	};
	struct OnceTask : public BaseContainer {
		OnceTask(std::shared_ptr<CGameTask> task, int frame) :
			BaseContainer(task), frame(frame) {}
		int frame;
	};
	std::list<OnceTask> onceTasks;

	struct RepeatTask : public BaseContainer {
		RepeatTask(std::shared_ptr<CGameTask> task, int frameInterval, int lastFrame) :
			BaseContainer(task), frameInterval(frameInterval), lastFrame(lastFrame) {}
		int frameInterval;
		int lastFrame;
	};
	std::list<RepeatTask> repeatTasks;

	std::list<std::shared_ptr<CGameTask>> removeTasks;

	struct WorkTask : public BaseContainer {
		WorkTask(std::weak_ptr<CScheduler> scheduler, std::shared_ptr<CGameTask> task, std::shared_ptr<CGameTask> onComplete) :
			BaseContainer(task), onComplete(onComplete), scheduler(scheduler) {}
		std::shared_ptr<CGameTask> onComplete;
		std::weak_ptr<CScheduler> scheduler;
	};
	static CMultiQueue<WorkTask> workTasks;

	struct FinishTask : public BaseContainer {
		FinishTask(std::shared_ptr<CGameTask> task) :
			BaseContainer(task) {}
		FinishTask(const WorkTask& workTask) :
			BaseContainer(workTask.onComplete) {}
	};
	CMultiQueue<FinishTask> finishTasks;

	static std::thread workerThread;
	static std::atomic<bool> workerRunning;
	static unsigned int counterInstance;

	static void WorkerThread();
};

} // namespace circuit

#endif // SCHEDULER_H_
