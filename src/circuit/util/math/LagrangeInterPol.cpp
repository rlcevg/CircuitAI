/*
 * LagrangeInterPol.cpp
 *
 *  Created on: Mar 21, 2015
 *      Author: rlcevg
 */

#include "util/math/LagrangeInterPol.h"
#include "util/Utils.h"

#include <assert.h>

namespace circuit {

CLagrangeInterPol::CLagrangeInterPol(const Vector& X, const Vector& Y)
{
	assert(X.size() == Y.size());
	xs = X;
	ys = Y;

	int n = ys.size();
	ts.reserve(n);
	for (int i = 0; i < n; ++i) {
		float t = 1.0;
		for (int j = 0; j < n; j++) {
			t *=  (j != i) ? (xs[i] - xs[j]) : 1.0;
		}
		ts.push_back(1.0 / t);
	}
}

CLagrangeInterPol::~CLagrangeInterPol()
{
}

float CLagrangeInterPol::GetValueAt(float x)
{
	float val = 0.0;
	int n = ys.size();
	for (int i = 0; i < n; ++i) {
		float s = 1.0;
		for (int j = 0; j < n; ++j) {
			if (j != i) {
				s *= x - xs[j];
			}
		}
		val += (s * ts[i]) * ys[i];
	}
	return val;
}

} // namespace circuit
