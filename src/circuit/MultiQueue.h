/*
 * MultiQueue.h
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#ifndef MULTIQUEUE_H_
#define MULTIQUEUE_H_

#include <list>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace circuit {

template <typename T>
class CMultiQueue {
public:
	typedef std::function<void (T& item)> ProcessFunction;
	using ConditionFunction = std::function<bool (T& item)>;

	CMultiQueue() = default;
	CMultiQueue(const CMultiQueue&) = delete; // disable copying

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

	CMultiQueue& operator=(const CMultiQueue&) = delete; // disable assignment

private:
	std::list<T> _queue;
	std::mutex _mutex;
	std::condition_variable _cond;
};

} // namespace circuit

#include "MultiQueue.hpp"

#endif // MULTIQUEUE_H_
