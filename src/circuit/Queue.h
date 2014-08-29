/*
 * Queue.h
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#ifndef QUEUE_H_
#define QUEUE_H_

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace circuit {

template <typename T>
class CQueue {
public:
	typedef std::function<void (T& item)> ProcessFunction;

	CQueue() = default;
	CQueue(const CQueue&) = delete; // disable copying

	T Pop();
	void Pop(T& item);
	void Push(const T& item);
	bool IsEmpty();
	void PopAndProcess(ProcessFunction process);
	void Clear();

	CQueue& operator=(const CQueue&) = delete; // disable assignment

private:
	std::queue<T> _queue;
	std::mutex _mutex;
	std::condition_variable _cond;
};

} // namespace circuit

#include "Queue.hpp"

#endif // QUEUE_H_
