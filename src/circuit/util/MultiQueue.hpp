/*
 * MultiQueue.hpp
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 *      Origin: Juan Palacios (https://github.com/juanchopanza/cppblog/blob/master/Concurrency/Queue/Queue.h)
 */

#ifndef SRC_CIRCUIT_UTIL_MULTIQUEUE_H_
#	error "Don't include this file directly, include MultiQueue.h instead"
#endif

#include "util/MultiQueue.h"

namespace circuit {

template <typename T>
T CMultiQueue<T>::Pop()
{
	std::unique_lock<spring::mutex> mlock(_mutex);
	while (_queue.empty()) {
		_cond.wait(mlock);
	}

	auto val = _queue.front();
	_queue.pop_front();
	return val;
}

template <typename T>
void CMultiQueue<T>::Pop(T& item)
{
	std::unique_lock<spring::mutex> mlock(_mutex);

	_cond.wait(mlock, [this]() { return !_queue.empty(); });

	item = _queue.front();
	_queue.pop_front();
}

template <typename T>
void CMultiQueue<T>::Push(const T& item)
{
	std::unique_lock<spring::mutex> mlock(_mutex);
	_queue.push_back(item);
	mlock.unlock();
	_cond.notify_one();
}

template <typename T>
bool CMultiQueue<T>::IsEmpty()
{
	std::unique_lock<spring::mutex> mlock(_mutex);
	bool isEmpty = _queue.empty();
	mlock.unlock();
    return isEmpty;
}

template <typename T>
void CMultiQueue<T>::PopAndProcess(ProcessFunction process)
{
	std::unique_lock<spring::mutex> mlock(_mutex);
	if (!_queue.empty()) {
		auto item = _queue.front();
		_queue.pop_front();
		mlock.unlock();
		process(item);
	}
}

template <typename T>
void CMultiQueue<T>::RemoveAllIf(ConditionFunction condition)
{
	std::unique_lock<spring::mutex> mlock(_mutex);
	typename std::deque<T>::iterator iter = _queue.begin();
	while (iter != _queue.end()) {
		if (condition(*iter)) {
//			iter = _queue.erase(iter);  // NOTE: micro-opt
			*iter = _queue.back();
			_queue.pop_back();
		} else {
			++iter;
		}
	}
}

template <typename T>
void CMultiQueue<T>::Clear()
{
	std::unique_lock<spring::mutex> mlock(_mutex);
	_queue.clear();
}

} // namespace circuit
