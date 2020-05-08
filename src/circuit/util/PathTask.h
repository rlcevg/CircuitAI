/*
 * PathTask.h
 *
 *  Created on: May 3, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_PATHTASK_H_
#define SRC_CIRCUIT_UTIL_PATHTASK_H_

#include <functional>
#include <memory>

namespace circuit {

class IPathQuery;
using PathFunc = std::function<void (std::shared_ptr<IPathQuery> query, int threadNum)>;
using PathedFunc = std::function<void (std::shared_ptr<IPathQuery> query)>;

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_PATHTASK_H_
