#include <btcb/lib/utility.hpp>

#include <windows.h>

void btcb::work_thread_reprioritize ()
{
	auto SUCCESS (SetThreadPriority (GetCurrentThread (), THREAD_MODE_BACKGROUND_BEGIN));
}
