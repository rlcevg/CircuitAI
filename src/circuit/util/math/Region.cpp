/*
 * Region.cpp
 *
 *  Created on: May 27, 2022
 *      Author: rlcevg
 */

#include "util/math/Region.h"
#include "util/math/poly1tri.h"

namespace utils {

using namespace springai;

CPolygon::CPolygon(F3Vec&& points)
		: box(points[0].x, points[0].x, points[0].z, points[0].z)
		, verts(points)
		, center(ZeroVector)
{
	for (const AIFloat3& v : verts) {
		box.Add(v);
	}
	Triangulate::Process(verts, tris);
	CalcArea();
	for (const AIFloat3& p : verts) {
		center += p;
	}
	center /= verts.size();
}

CPolygon::CPolygon(const SBox& b)
		: box(b)
		, area((box.right - box.left) * (box.bottom - box.top))
		, center((AIFloat3(box.left, 0.f, box.top) + AIFloat3(box.right, 0.f, box.bottom)) / 2)
{
	verts.emplace_back(box.left, 0.f, box.top);
	verts.emplace_back(box.left, 0.f, box.bottom);
	verts.emplace_back(box.right, 0.f, box.bottom);
	verts.emplace_back(box.right, 0.f, box.top);
	tris = {0, 1, 2, 2, 3, 0};
	areas = {area / 2, area / 2};
}

bool CPolygon::ContainsPoint(const AIFloat3& point) const
{
	if (!box.ContainsPoint(point)) {
		return false;
	}

	// NOTE: collinear and edge cases broken
	bool in = false;
	for (int i = 0, j = verts.size() - 1; i < (int)verts.size(); j = i++) {
		if (((verts[i].z > point.z) != (verts[j].z > point.z))
			&& (point.x < (verts[j].x - verts[i].x) * (point.z - verts[i].z) / (verts[j].z - verts[i].z) + verts[i].x))
		{
			in = !in;
		}
	}

	return in;
}

AIFloat3 CPolygon::Random() const
{
	int choice = 0;
	float dice = (float)rand() / RAND_MAX * area;
	for (int i = 0; i < (int)areas.size(); ++i) {
		dice -= areas[i];
		if (dice < 0.f) {
			choice = i;
			break;
		}
	}
	AIFloat3 a = verts[tris[choice * 3 + 1]] - verts[tris[choice * 3 + 0]];
	AIFloat3 b = verts[tris[choice * 3 + 2]] - verts[tris[choice * 3 + 0]];
	float u1 = (float)rand() / RAND_MAX;
	float u2 = (float)rand() / RAND_MAX;
	if (u1 + u2 > 1.f) {  // parallelogram, reflection
		u1 = 1.f - u1;
		u2 = 1.f - u2;
	}
	AIFloat3 w = a * u1 + b * u2;
	return w + verts[tris[choice * 3 + 0]];
}

void CPolygon::Scale(float value)
{
	for (AIFloat3& p : verts) {
		p = (p - center) * value + center;
	}
	CalcArea();
}

void CPolygon::Extend(float value)
{
	for (AIFloat3& p : verts) {
		p = (p - center) * value + center;
	}
	CalcArea();
}

void CPolygon::CalcArea()
{
	const int tcount = tris.size() / 3;
	for (int i = 0; i < tcount; ++i) {
		areas.push_back(Triangulate::Area(verts[tris[i * 3 + 0]], verts[tris[i * 3 + 1]], verts[tris[i * 3 + 2]]));
	}
	area = Triangulate::Area(verts);
}

CRegion::CRegion(std::vector<CPolygon>&& polys)
		: box(polys[0].GetBox())
		, parts(polys)
		, area(0.f)
{
	for (const CPolygon& p : parts) {
		box.Merge(p.GetBox());
		area += p.GetArea();
	}
}

CRegion::CRegion(SBox&& b)
		: box(b)
{
	parts.emplace_back(box);
	area = parts[0].GetArea();
}

bool CRegion::ContainsPoint(const AIFloat3& point) const
{
	if (!box.ContainsPoint(point)) {
		return false;
	}
	for (const CPolygon& poly : parts) {
		if (poly.ContainsPoint(point)) {
			return true;
		}
	}
	return false;
}

AIFloat3 CRegion::Random() const
{
	float dice = (float)rand() / RAND_MAX * area;
	for (const CPolygon& p : parts) {
		dice -= p.GetArea();
		if (dice < 0.f) {
			return p.Random();
		}
	}
	return parts.back().Random();
}

} // namespace utils
