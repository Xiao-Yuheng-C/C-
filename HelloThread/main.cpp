#include <iostream>
#include <thread>
#include "CELLTimestamp.hpp"
#include <atomic>

using namespace std;

const int tCount = 4;
atomic_int sum2 = 0;

void workFun(int *res) {
	for (int i = 0; i < 20000000; i++) {
		++(*res);
	}
	return;
}

void workFun2() {
	for (int i = 0; i < 20000000; i++) {
		++sum2;
	}
	return;
}

int main() {
	thread *t[tCount];
	int a[tCount] = { 0 }, sum = 0;
	CELLTimestamp tTime;
	for (int i = 0; i < tCount; i++) {
		t[i] = new thread(workFun, &a[i]);
	}
	for (int i = 0; i < tCount; i++) {
		t[i]->join();
	}
	for (int i = 0; i < tCount; i++) {
		sum += a[i];
	}
	cout << tTime.getElapsedSecond() << ", sum=" << sum << endl;
	sum = 0;
	tTime.update();
	for (int i = 0; i < 80000000; i++) {
		++sum2;
	}
	cout << tTime.getElapsedSecond() << ", sum=" << sum2 << endl;
	return 0;
}