/*
 * MultiQueue.h
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_MULTIQUEUE_H_
#define SRC_CIRCUIT_UTIL_MULTIQUEUE_H_

#include "System/Threading/SpringThreading.h"

#include <deque>
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
	 * Pop object from queue if any exists and return it,
	 * otherwise wait for object to appear in queue.
	 */
	T Pop();
	/*
	 * @see Pop()
	 */
	void Pop(T& item);
	void Push(const T& item);
	bool IsEmpty();
	size_t Size();
	/*
	 * Pop object if any exists in queue and process it, quit immediately otherwise.
	 * Return true if object was processed.
	 */
	bool PopAndProcess(ProcessFunction process);
	/*
	 * Pop all objects in queue and process it.
	 */
	void PopAndProcessAll(ProcessFunction process);
	/*
	 * Remove all elements for which condition is true.
	 */
	void RemoveAllIf(ConditionFunction condition);
	void Clear();

	CMultiQueue& operator=(const CMultiQueue&) = delete; // disable assignment

private:
	std::deque<T> _queue;
	spring::mutex _mutex;
	spring::condition_variable_any _cond;
};

} // namespace circuit

#include "util/MultiQueue.hpp"

#endif // SRC_CIRCUIT_UTIL_MULTIQUEUE_H_
