/*
 * StartBox.h
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#ifndef STARTBOX_H_
#define STARTBOX_H_

#include <sys/types.h>
#include <memory>
#include <array>
#include <vector>

namespace circuit {

enum class BoxEdges: int {BOTTOM = 0, LEFT = 1, RIGHT = 2, TOP = 3};

typedef std::array<float, 4> Box;
//using Box = std::array<float, 4>;

class CStartBox {
public:
	CStartBox(const char* setupScript, int width, int height);
	virtual ~CStartBox();

	static void CreateInstance(const char* setupScript, int width, int height);
	static CStartBox& GetInstance();
	static void DestroyInstance();

	bool IsEmpty();

	const std::array<float, 4>& operator[](int idx) const;

private:
	static std::unique_ptr<CStartBox> singleton;
	static uint counter;

	std::vector<Box> boxes;
};

#define startBoxes CStartBox::GetInstance()

} // namespace circuit

#endif // STARTBOX_H_
