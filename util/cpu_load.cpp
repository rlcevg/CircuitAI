#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <math.h>

constexpr int NUM_THREADS = 4;
static std::vector<std::thread> threads;
static std::atomic<bool> workerRunning(true);

//This function will be called from a thread
void call_from_thread() {
	float x = 1.5f;
	long i = 0;
	while (workerRunning) {
		x *= sin(x) / atan(x) * tanh(x) * sqrt(x);
		if (++i % 10000000 == 0) {
			std::cout << x << "\n";
		}
	}
}

int main() {
	//Launch a thread
	for (int i = 0; i < NUM_THREADS; ++i) {
		threads.push_back(std::move(std::thread(call_from_thread)));
	}

	std::cin.get();
	workerRunning = false;
	
	//Join the thread with the main thread
	for (std::thread& t : threads) {
		t.join();
	}

	return 0;
}
