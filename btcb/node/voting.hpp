#pragma once

#include <btcb/lib/numbers.hpp>

#include <boost/thread.hpp>

#include <condition_variable>
#include <deque>
#include <mutex>

namespace btcb
{
class node;
class vote_generator
{
public:
	vote_generator (btcb::node &, std::chrono::milliseconds);
	void add (btcb::block_hash const &);
	void stop ();

private:
	void run ();
	void send (std::unique_lock<std::mutex> &);
	btcb::node & node;
	std::mutex mutex;
	std::condition_variable condition;
	std::deque<btcb::block_hash> hashes;
	std::chrono::milliseconds wait;
	bool stopped;
	bool started;
	boost::thread thread;
};
}
