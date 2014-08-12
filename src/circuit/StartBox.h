/*
 * StartBox.h
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#ifndef STARTBOX_H_
#define STARTBOX_H_

#include <array>
#include <vector>

namespace circuit {

enum class BoxEdges: int {BOTTOM = 0, LEFT = 1, RIGHT = 2, TOP = 3};

//typedef std::array<float, 4> Box;
using Box = std::array<float, 4>;

class CStartBox {
public:
	CStartBox(const char* setupScript, int width, int height);
	virtual ~CStartBox();

	bool IsEmpty();

	const Box& operator[](int idx) const;

private:
	std::vector<Box> boxes;
};

} // namespace circuit

#endif // STARTBOX_H_
