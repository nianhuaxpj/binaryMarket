#include"market.h"
#include<thread>
#include<chrono>
using namespace std;
int main()
{
	std::thread([] {
		Market mm;
		mm.start();
	}).detach();
	for (;;)
		this_thread::sleep_for(1s);
}