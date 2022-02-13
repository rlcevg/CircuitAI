/*
 * Data.h
 *
 *  Created on: Feb 13, 2022
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_DATA_H_
#define SRC_CIRCUIT_UTIL_DATA_H_

//#undef NDEBUG
#include <cassert>
#include <algorithm>
#include <cmath>

namespace utils {

template <class T> void unused(const T &) {}

inline int queenWiseNorm(int dx, int dy)
{
	return std::max(std::abs(dx), std::abs(dy));
}

inline int squaredNorm(int dx, int dy)
{
	return dx * dx + dy * dy;
}

inline double norm(int dx, int dy)
{
	return std::sqrt(squaredNorm(dx, dy));
}

template<class T>
inline void really_remove(T & Container, const typename T::value_type & Element)
{
	Container.erase(std::remove(Container.begin(), Container.end(), Element), Container.end());
}

template<class T, class Pred>
inline void really_remove_if(T& Container, Pred pred)
{
	Container.erase(std::remove_if(Container.begin(), Container.end(), pred), Container.end());
}

template<class T, class E>
inline bool contains(const T& Container, const E& Element)
{
	return std::find(Container.begin(), Container.end(), Element) != Container.end();
}

template<class T>
inline void fast_erase(std::vector<T>& Vector, int i)
{
	assert((0 <= i) && (i < (int)Vector.size()));

	std::swap(Vector[i], Vector.back());
	Vector.pop_back();
}

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Markable
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//
//  Provides efficient marking ability.
//  Usage: class MyNode : (public) Markable<MyNode, unsigned> {...};
//  Note: This implementation uses a static member.
//
template<class Derived, class Mark>
class Markable {
public:
	typedef Mark mark_t;

					Markable() : m_lastMark(0) {}

	bool			Marked() const		{ return m_lastMark == m_currentMark; }
	void			SetMarked() const	{ m_lastMark = m_currentMark; }
	void			SetUnmarked() const	{ m_lastMark = m_currentMark - 1; }
	static void		UnmarkAll()			{ ++m_currentMark; }

private:
	mutable mark_t	m_lastMark;
	static mark_t	m_currentMark;
};

template<class Derived, class Mark>
Mark Markable<Derived, Mark>::m_currentMark = 0;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class UserData
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//
//  Provides free-to-use, intrusive data for several types of the BWEM library
//  Despite their names and their types, they can be used for any purpose.
//
class UserData {
public:
	// Free use.
	int					Data() const					{ return m_data; }
	void				SetData(int data) const			{ m_data = data; }

	// Free use.
	void *				Ptr() const						{ return m_ptr; }
	void				SetPtr(void * p) const			{ m_ptr = p; }

	// Free use.
	void *				Ext() const						{ return m_ext; }
	void				SetExt(void * p) const			{ m_ext = p; }

protected:
						UserData() = default;
						UserData(const UserData &) = default;

private:
	mutable void *		m_ptr = nullptr;
	mutable void *		m_ext = nullptr;
	mutable int			m_data = 0;
};

}  // namespace utils

#endif // SRC_CIRCUIT_UTIL_DATA_H_
