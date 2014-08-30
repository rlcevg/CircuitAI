/*
 * Queue.h
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#ifndef QUEUE_H_
#define QUEUE_H_

#include <list>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace circuit {

template <typename T>
class CQueue {
public:
	typedef std::function<void (T& item)> ProcessFunction;
	typedef std::function<bool (T& item)> ConditionFunction;

	CQueue() = default;
	CQueue(const CQueue&) = delete; // disable copying

	/*
	 * Pop object from queue if any exists and return it, otherwise wait for object to appear in queue
	 */
	T Pop();
	/*
	 * @see Pop()
	 */
	void Pop(T& item);
	void Push(const T& item);
	bool IsEmpty();
	/*
	 * Pop object if any exists in queue and process it, quit immediately otherwise
	 */
	void PopAndProcess(ProcessFunction process);
	/*
	 * Remove all elements for which condition is true
	 */
	void RemoveAllIf(ConditionFunction condition);
	void Clear();

	CQueue& operator=(const CQueue&) = delete; // disable assignment

private:
	std::list<T> _queue;
	std::mutex _mutex;
	std::condition_variable _cond;
};

} // namespace circuit

#include "Queue.hpp"

#endif // QUEUE_H_
