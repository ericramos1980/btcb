#pragma once

#include <boost/optional.hpp>
#include <boost/thread/thread.hpp>
#include <btcb/lib/config.hpp>
#include <btcb/lib/numbers.hpp>
#include <btcb/lib/utility.hpp>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <thread>

namespace btcb
{
class block;
bool work_validate (btcb::block_hash const &, uint64_t, uint64_t * = nullptr);
bool work_validate (btcb::block const &, uint64_t * = nullptr);
uint64_t work_value (btcb::block_hash const &, uint64_t);
class opencl_work;
class work_item
{
public:
	btcb::uint256_union item;
	std::function<void(boost::optional<uint64_t> const &)> callback;
	uint64_t difficulty;
};
class work_pool
{
public:
	work_pool (unsigned, std::function<boost::optional<uint64_t> (btcb::uint256_union const &)> = nullptr);
	~work_pool ();
	void loop (uint64_t);
	void stop ();
	void cancel (btcb::uint256_union const &);
	void generate (btcb::uint256_union const &, std::function<void(boost::optional<uint64_t> const &)>, uint64_t = btcb::work_pool::publish_threshold);
	uint64_t generate (btcb::uint256_union const &, uint64_t = btcb::work_pool::publish_threshold);
	std::atomic<int> ticket;
	bool done;
	std::vector<boost::thread> threads;
	std::list<btcb::work_item> pending;
	std::mutex mutex;
	std::condition_variable producer_condition;
	std::function<boost::optional<uint64_t> (btcb::uint256_union const &)> opencl;
	btcb::observer_set<bool> work_observers;
	// Local work threshold for rate-limiting publishing blocks. ~5 seconds of work.
	static uint64_t const publish_test_threshold = 0xff00000000000000;
	static uint64_t const publish_full_threshold = 0xffffffc000000000;
	static uint64_t const publish_threshold = btcb::btcb_network == btcb::btcb_networks::btcb_test_network ? publish_test_threshold : publish_full_threshold;
};
}
