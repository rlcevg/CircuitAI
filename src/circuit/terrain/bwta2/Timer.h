#pragma once

#if defined(_MSCV) || defined(_MSC_VER) || defined(WIN32) || defined(_WINDOWS)

#include <windows.h>	// for Windows APIs

class Timer
{
private:
	LARGE_INTEGER lFreq, lStart;
	LONGLONG elapsedTime;

public:
	Timer()
		: elapsedTime(0)
	{
		QueryPerformanceFrequency(&lFreq);
	}

	inline void start()
	{
		QueryPerformanceCounter(&lStart);
		elapsedTime = 0;
	}

	inline void stop()
	{
		LARGE_INTEGER lEnd;
		QueryPerformanceCounter(&lEnd);
		elapsedTime += lEnd.QuadPart - lStart.QuadPart;
	}

	// Return duration in seconds
	inline double getElapsedTime()
	{
		return (double(elapsedTime) / lFreq.QuadPart);
	}

	// Return duration in seconds
	inline double stopAndGetTime()
	{
		stop();
		return getElapsedTime();
	}
};

#else

#include <time.h>

class Timer
{
private:
  inline double getSeconds(struct timespec t)
  {
    return t.tv_sec + t.tv_nsec * (double)(1e-9);
  }

  struct timespec tStart;
  struct timespec tEnd;
  double elapsedTime;

public:
  Timer()
    : elapsedTime((double)0)
  {

  }

  inline void start()
  {
    clock_gettime(CLOCK_MONOTONIC, &tStart);
    elapsedTime = (double)0;
  }

  inline void stop()
  {
    clock_gettime(CLOCK_MONOTONIC, &tEnd);
  }

  inline double getElapsedTime()
  {
    return (getSeconds(tEnd) - getSeconds(tStart));
  }

  inline double stopAndGetTime()
  {
    stop();
    return getElapsedTime();
  }
};

#endif
