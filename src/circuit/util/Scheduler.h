/*
 * Scheduler.h
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_SCHEDULER_H_
#define SRC_CIRCUIT_UTIL_SCHEDULER_H_

#include "util/MultiQueue.h"
#include "util/GameTask.h"
#include "util/Defines.h"

#include "System/Threading/SpringThreading.h"

#include <memory>
#include <list>

namespace circuit {

class CScheduler {
public:
	CScheduler();
	virtual ~CScheduler();

	void Init(const std::shared_ptr<CScheduler>& thisPtr) { self = thisPtr; }
	void ProcessInit();
	void ProcessRelease();

private:
	void Release();

public:
	/*
	 * Add task at specified frame, or execute immediately at next frame
	 */
	void RunTaskAt(std::shared_ptr<CGameTask> task, int frame = 0) {
		onceTasks.push_back({task, frame});
	}

	/*
	 * Add task at frame relative to current frame
	 */
	void RunTaskAfter(std::shared_ptr<CGameTask> task, int frame = 0) {
		onceTasks.push_back({task, lastFrame + frame});
	}

	/*
	 * Add task at specified interval
	 */
	void RunTaskEvery(std::shared_ptr<CGameTask> task, int frameInterval = FRAMES_PER_SEC, int frameOffset = 0);

	/*
	 * Process queued tasks at specified frame
	 */
	void ProcessTasks(int frame);

	/*
	 * Run concurrent task, finalize on success at main thread
	 */
	void RunParallelTask(std::shared_ptr<CGameTask> task, std::shared_ptr<CGameTask> onComplete = nullptr);

	/*
	 * Run concurrent pathfinder, finalize on complete at main thread
	 */
	void RunPathTask(std::shared_ptr<CGameTask> task, std::shared_ptr<CGameTask> onComplete = nullptr);

	/*
	 * Remove scheduled task from queue
	 */
	void RemoveTask(std::shared_ptr<CGameTask>& task);

	/*
	 * Run task on init. Not affected by RemoveTask
	 */
	void RunOnInit(std::shared_ptr<CGameTask> task) {
		initTasks.push_back(task);
	}

	/*
	 * Run task on release. Not affected by RemoveTask
	 */
	void RunOnRelease(std::shared_ptr<CGameTask> task) {
		releaseTasks.push_back(task);
	}

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
	struct OnceTask: public BaseContainer {
		OnceTask(std::shared_ptr<CGameTask> task, int frame) :
			BaseContainer(task), frame(frame) {}
		int frame;
	};
	std::list<OnceTask> onceTasks;

	struct RepeatTask: public BaseContainer {
		RepeatTask(std::shared_ptr<CGameTask> task, int frameInterval, int lastFrame) :
			BaseContainer(task), frameInterval(frameInterval), lastFrame(lastFrame) {}
		int frameInterval;
		int lastFrame;
	};
	std::list<RepeatTask> repeatTasks;

	std::vector<std::shared_ptr<CGameTask>> removeTasks;

	struct WorkTask: public BaseContainer {
		WorkTask(std::weak_ptr<CScheduler> scheduler, std::shared_ptr<CGameTask> task, std::shared_ptr<CGameTask> onComplete) :
			BaseContainer(task), onComplete(onComplete), scheduler(scheduler) {}
		std::shared_ptr<CGameTask> onComplete;
		std::weak_ptr<CScheduler> scheduler;
	};
	static CMultiQueue<WorkTask> workTasks;
	static CMultiQueue<WorkTask> pathTasks;

	struct FinishTask: public BaseContainer {
		FinishTask(std::shared_ptr<CGameTask> task) :
			BaseContainer(task) {}
		FinishTask(const WorkTask& workTask) :
			BaseContainer(workTask.onComplete) {}
	};
	CMultiQueue<FinishTask> workedTasks;  // onComplete
	CMultiQueue<FinishTask> pathedTasks;  // onComplete

	std::vector<std::shared_ptr<CGameTask>> initTasks;
	std::vector<std::shared_ptr<CGameTask>> releaseTasks;

	static spring::thread workerThread;
	static spring::thread patherThread;
	static std::atomic<bool> workerRunning;
	static unsigned int counterInstance;

	static void WorkerThread();
	static void PatherThread();
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_SCHEDULER_H_
