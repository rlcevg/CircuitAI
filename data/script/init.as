#include "role.as"


int FibR(int n)
{
	if (n < 2) return n;
	return (FibR(n-2) + FibR(n-1));
}

void Init() {
//	for (int i = 0; i < 10; ++i) {
//		FibR(30);
//		aiLog("AngelScript Rules! " + i);
//	}
	aiLog("AngelScript Rules!");
}
