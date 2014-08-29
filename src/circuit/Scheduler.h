/*
 * Scheduler.h
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include "Task.h"
#include "Queue.h"

#include <list>
#include <memory>
#include <thread>
#include <atomic>

namespace circuit {

class CScheduler {
public:
	CScheduler();
	virtual ~CScheduler();

	void Release();

	/*
	 * Add task at specified frame, or execute immediately at next frame
	 */
	void RunTaskAt(std::shared_ptr<CTask> task, int frame = 0);

	/*
	 * Add task at specified interval
	 */
	void RunTaskEvery(std::shared_ptr<CTask> task, int frameInterval = 30);

	/*
	 * Process queued tasks at specified frame
	 */
	void ProcessTasks(int frame);

	/*
	 * Run concurrent task, finalize on success at main thread
	 */
	void RunParallelTask(std::shared_ptr<CTask> task, std::shared_ptr<CTask> onSuccess = nullptr);

	/*
	 * Remove scheduled task from queue
	 */
	void RemoveTask(std::shared_ptr<CTask> task);

public:
	struct BaseContainer {
		BaseContainer(std::shared_ptr<CTask> task) :
			task(task) {}
		std::shared_ptr<CTask> task;
		bool operator==(const BaseContainer& other) const  {
			return task == other.task;
		}
	};
	struct OnceTask : public BaseContainer {
		OnceTask(std::shared_ptr<CTask> task, int frame) :
			BaseContainer(task), frame(frame) {}
		int frame;
	};
	std::list<OnceTask> onceTasks;

	struct IntervalTask : public BaseContainer {
		IntervalTask(std::shared_ptr<CTask> task, int frameInterval, int lastFrame) :
			BaseContainer(task), frameInterval(frameInterval), lastFrame(lastFrame) {}
		int frameInterval;
		int lastFrame;
	};
	std::list<IntervalTask> intervalTasks;

	struct WorkTask : public BaseContainer {
		WorkTask(CScheduler* scheduler, std::shared_ptr<CTask> task, std::shared_ptr<CTask> onSuccess) :
			BaseContainer(task), onSuccess(onSuccess), scheduler(scheduler) {}
		std::shared_ptr<CTask> onSuccess;
		CScheduler* scheduler;
	};
	static CQueue<WorkTask> workTasks;

	struct FinishTask : public BaseContainer {
		FinishTask(std::shared_ptr<CTask> task) :
			BaseContainer(task) {}
		FinishTask(const WorkTask& workTask) :
			BaseContainer(workTask.onSuccess) {}
	};
	CQueue<FinishTask> finishTasks;

	static std::thread workerThread;
	static std::atomic<bool> workerRunning;

	static void WorkerThread();
};

} // namespace circuit

#endif // SCHEDULER_H_
