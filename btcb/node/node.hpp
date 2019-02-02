#pragma once

#include <btcb/lib/work.hpp>
#include <btcb/node/bootstrap.hpp>
#include <btcb/node/logging.hpp>
#include <btcb/node/nodeconfig.hpp>
#include <btcb/node/peers.hpp>
#include <btcb/node/portmapping.hpp>
#include <btcb/node/stats.hpp>
#include <btcb/node/voting.hpp>
#include <btcb/node/wallet.hpp>
#include <btcb/secure/ledger.hpp>

#include <condition_variable>

#include <boost/iostreams/device/array.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/thread/thread.hpp>

namespace btcb
{
class node;
class election_status
{
public:
	std::shared_ptr<btcb::block> winner;
	btcb::amount tally;
	std::chrono::milliseconds election_end;
	std::chrono::milliseconds election_duration;
};
class vote_info
{
public:
	std::chrono::steady_clock::time_point time;
	uint64_t sequence;
	btcb::block_hash hash;
};
class election_vote_result
{
public:
	election_vote_result ();
	election_vote_result (bool, bool);
	bool replay;
	bool processed;
};
class election : public std::enable_shared_from_this<btcb::election>
{
	std::function<void(std::shared_ptr<btcb::block>)> confirmation_action;
	void confirm_once (btcb::transaction const &);
	void confirm_back (btcb::transaction const &);

public:
	election (btcb::node &, std::shared_ptr<btcb::block>, std::function<void(std::shared_ptr<btcb::block>)> const &);
	btcb::election_vote_result vote (btcb::account, uint64_t, btcb::block_hash);
	btcb::tally_t tally (btcb::transaction const &);
	// Check if we have vote quorum
	bool have_quorum (btcb::tally_t const &, btcb::uint128_t);
	// Change our winner to agree with the network
	void compute_rep_votes (btcb::transaction const &);
	// Confirm this block if quorum is met
	void confirm_if_quorum (btcb::transaction const &);
	void log_votes (btcb::tally_t const &);
	bool publish (std::shared_ptr<btcb::block> block_a);
	void stop ();
	btcb::node & node;
	std::unordered_map<btcb::account, btcb::vote_info> last_votes;
	std::unordered_map<btcb::block_hash, std::shared_ptr<btcb::block>> blocks;
	btcb::block_hash root;
	std::chrono::steady_clock::time_point election_start;
	btcb::election_status status;
	std::atomic<bool> confirmed;
	bool stopped;
	std::unordered_map<btcb::block_hash, btcb::uint128_t> last_tally;
	unsigned announcements;
};
class conflict_info
{
public:
	btcb::block_hash root;
	uint64_t difficulty;
	std::shared_ptr<btcb::election> election;
};
// Core class for determining consensus
// Holds all active blocks i.e. recently added blocks that need confirmation
class active_transactions
{
public:
	active_transactions (btcb::node &);
	~active_transactions ();
	// Start an election for a block
	// Call action with confirmed block, may be different than what we started with
	bool start (std::shared_ptr<btcb::block>, std::function<void(std::shared_ptr<btcb::block>)> const & = [](std::shared_ptr<btcb::block>) {});
	// If this returns true, the vote is a replay
	// If this returns false, the vote may or may not be a replay
	bool vote (std::shared_ptr<btcb::vote>, bool = false);
	// Is the root of this block in the roots container
	bool active (btcb::block const &);
	void update_difficulty (btcb::block const &);
	std::deque<std::shared_ptr<btcb::block>> list_blocks (bool = false);
	void erase (btcb::block const &);
	void stop ();
	bool publish (std::shared_ptr<btcb::block> block_a);
	boost::multi_index_container<
	btcb::conflict_info,
	boost::multi_index::indexed_by<
	boost::multi_index::hashed_unique<
	boost::multi_index::member<btcb::conflict_info, btcb::block_hash, &btcb::conflict_info::root>>,
	boost::multi_index::ordered_non_unique<
	boost::multi_index::member<btcb::conflict_info, uint64_t, &btcb::conflict_info::difficulty>,
	std::greater<uint64_t>>>>
	roots;
	std::unordered_map<btcb::block_hash, std::shared_ptr<btcb::election>> blocks;
	std::deque<btcb::election_status> confirmed;
	btcb::node & node;
	std::mutex mutex;
	// Maximum number of conflicts to vote on per interval, lowest root hash first
	static unsigned constexpr announcements_per_interval = 32;
	// Minimum number of block announcements
	static unsigned constexpr announcement_min = 2;
	// Threshold to start logging blocks haven't yet been confirmed
	static unsigned constexpr announcement_long = 20;
	static unsigned constexpr announce_interval_ms = (btcb::btcb_network == btcb::btcb_networks::btcb_test_network) ? 10 : 16000;
	static size_t constexpr election_history_size = 2048;
	static size_t constexpr max_broadcast_queue = 1000;

private:
	// Call action with confirmed block, may be different than what we started with
	bool add (std::shared_ptr<btcb::block>, std::function<void(std::shared_ptr<btcb::block>)> const & = [](std::shared_ptr<btcb::block>) {});
	void announce_loop ();
	void announce_votes (std::unique_lock<std::mutex> &);
	std::condition_variable condition;
	bool started;
	bool stopped;
	boost::thread thread;
};
class operation
{
public:
	bool operator> (btcb::operation const &) const;
	std::chrono::steady_clock::time_point wakeup;
	std::function<void()> function;
};
class alarm
{
public:
	alarm (boost::asio::io_context &);
	~alarm ();
	void add (std::chrono::steady_clock::time_point const &, std::function<void()> const &);
	void run ();
	boost::asio::io_context & io_ctx;
	std::mutex mutex;
	std::condition_variable condition;
	std::priority_queue<operation, std::vector<operation>, std::greater<operation>> operations;
	boost::thread thread;
};
class gap_information
{
public:
	std::chrono::steady_clock::time_point arrival;
	btcb::block_hash hash;
	std::unordered_set<btcb::account> voters;
};
class gap_cache
{
public:
	gap_cache (btcb::node &);
	void add (btcb::transaction const &, std::shared_ptr<btcb::block>);
	void vote (std::shared_ptr<btcb::vote>);
	btcb::uint128_t bootstrap_threshold (btcb::transaction const &);
	boost::multi_index_container<
	btcb::gap_information,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::member<gap_information, std::chrono::steady_clock::time_point, &gap_information::arrival>>,
	boost::multi_index::hashed_unique<boost::multi_index::member<gap_information, btcb::block_hash, &gap_information::hash>>>>
	blocks;
	size_t const max = 256;
	std::mutex mutex;
	btcb::node & node;
};
class work_pool;
class send_info
{
public:
	uint8_t const * data;
	size_t size;
	btcb::endpoint endpoint;
	std::function<void(boost::system::error_code const &, size_t)> callback;
};
class block_arrival_info
{
public:
	std::chrono::steady_clock::time_point arrival;
	btcb::block_hash hash;
};
// This class tracks blocks that are probably live because they arrived in a UDP packet
// This gives a fairly reliable way to differentiate between blocks being inserted via bootstrap or new, live blocks.
class block_arrival
{
public:
	// Return `true' to indicated an error if the block has already been inserted
	bool add (btcb::block_hash const &);
	bool recent (btcb::block_hash const &);
	boost::multi_index_container<
	btcb::block_arrival_info,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::member<btcb::block_arrival_info, std::chrono::steady_clock::time_point, &btcb::block_arrival_info::arrival>>,
	boost::multi_index::hashed_unique<boost::multi_index::member<btcb::block_arrival_info, btcb::block_hash, &btcb::block_arrival_info::hash>>>>
	arrival;
	std::mutex mutex;
	static size_t constexpr arrival_size_min = 8 * 1024;
	static std::chrono::seconds constexpr arrival_time_min = std::chrono::seconds (300);
};
class rep_last_heard_info
{
public:
	std::chrono::steady_clock::time_point last_heard;
	btcb::account representative;
};
class online_reps
{
public:
	online_reps (btcb::node &);
	void vote (std::shared_ptr<btcb::vote> const &);
	void recalculate_stake ();
	btcb::uint128_t online_stake ();
	btcb::uint128_t online_stake_total;
	std::vector<btcb::account> list ();
	boost::multi_index_container<
	btcb::rep_last_heard_info,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::member<btcb::rep_last_heard_info, std::chrono::steady_clock::time_point, &btcb::rep_last_heard_info::last_heard>>,
	boost::multi_index::hashed_unique<boost::multi_index::member<btcb::rep_last_heard_info, btcb::account, &btcb::rep_last_heard_info::representative>>>>
	reps;

private:
	std::mutex mutex;
	btcb::node & node;
};
class udp_data
{
public:
	uint8_t * buffer;
	size_t size;
	btcb::endpoint endpoint;
};
/**
  * A circular buffer for servicing UDP datagrams. This container follows a producer/consumer model where the operating system is producing data in to buffers which are serviced by internal threads.
  * If buffers are not serviced fast enough they're internally dropped.
  * This container has a maximum space to hold N buffers of M size and will allocate them in round-robin order.
  * All public methods are thread-safe
*/
class udp_buffer
{
public:
	// Stats - Statistics
	// Size - Size of each individual buffer
	// Count - Number of buffers to allocate
	udp_buffer (btcb::stat & stats, size_t, size_t);
	// Return a buffer where UDP data can be put
	// Method will attempt to return the first free buffer
	// If there are no free buffers, an unserviced buffer will be dequeued and returned
	// Function will block if there are no free or unserviced buffers
	// Return nullptr if the container has stopped
	btcb::udp_data * allocate ();
	// Queue a buffer that has been filled with UDP data and notify servicing threads
	void enqueue (btcb::udp_data *);
	// Return a buffer that has been filled with UDP data
	// Function will block until a buffer has been added
	// Return nullptr if the container has stopped
	btcb::udp_data * dequeue ();
	// Return a buffer to the freelist after is has been serviced
	void release (btcb::udp_data *);
	// Stop container and notify waiting threads
	void stop ();

private:
	btcb::stat & stats;
	std::mutex mutex;
	std::condition_variable condition;
	boost::circular_buffer<btcb::udp_data *> free;
	boost::circular_buffer<btcb::udp_data *> full;
	std::vector<uint8_t> slab;
	std::vector<btcb::udp_data> entries;
	bool stopped;
};
class network
{
public:
	network (btcb::node &, uint16_t);
	~network ();
	void receive ();
	void process_packets ();
	void start ();
	void stop ();
	void receive_action (btcb::udp_data *);
	void rpc_action (boost::system::error_code const &, size_t);
	void republish_vote (std::shared_ptr<btcb::vote>);
	void republish_block (std::shared_ptr<btcb::block>);
	static unsigned const broadcast_interval_ms = 10;
	void republish_block_batch (std::deque<std::shared_ptr<btcb::block>>, unsigned = broadcast_interval_ms);
	void republish (btcb::block_hash const &, std::shared_ptr<std::vector<uint8_t>>, btcb::endpoint);
	void confirm_send (btcb::confirm_ack const &, std::shared_ptr<std::vector<uint8_t>>, btcb::endpoint const &);
	void merge_peers (std::array<btcb::endpoint, 8> const &);
	void send_keepalive (btcb::endpoint const &);
	void send_node_id_handshake (btcb::endpoint const &, boost::optional<btcb::uint256_union> const & query, boost::optional<btcb::uint256_union> const & respond_to);
	void broadcast_confirm_req (std::shared_ptr<btcb::block>);
	void broadcast_confirm_req_base (std::shared_ptr<btcb::block>, std::shared_ptr<std::vector<btcb::peer_information>>, unsigned, bool = false);
	void broadcast_confirm_req_batch (std::deque<std::pair<std::shared_ptr<btcb::block>, std::shared_ptr<std::vector<btcb::peer_information>>>>, unsigned = broadcast_interval_ms);
	void send_confirm_req (btcb::endpoint const &, std::shared_ptr<btcb::block>);
	void send_buffer (uint8_t const *, size_t, btcb::endpoint const &, std::function<void(boost::system::error_code const &, size_t)>);
	btcb::endpoint endpoint ();
	btcb::udp_buffer buffer_container;
	boost::asio::ip::udp::socket socket;
	std::mutex socket_mutex;
	boost::asio::ip::udp::resolver resolver;
	std::vector<boost::thread> packet_processing_threads;
	btcb::node & node;
	bool on;
    static uint16_t node_port;
    static size_t const buffer_size = 512;
};

class node_init
{
public:
	node_init ();
	bool error ();
	bool block_store_init;
	bool wallet_init;
};
class node_observers
{
public:
	btcb::observer_set<std::shared_ptr<btcb::block>, btcb::account const &, btcb::uint128_t const &, bool> blocks;
	btcb::observer_set<bool> wallet;
	btcb::observer_set<btcb::transaction const &, std::shared_ptr<btcb::vote>, btcb::endpoint const &> vote;
	btcb::observer_set<btcb::account const &, bool> account_balance;
	btcb::observer_set<btcb::endpoint const &> endpoint;
	btcb::observer_set<> disconnect;
};
class vote_processor
{
public:
	vote_processor (btcb::node &);
	void vote (std::shared_ptr<btcb::vote>, btcb::endpoint);
	// node.active.mutex lock required
	btcb::vote_code vote_blocking (btcb::transaction const &, std::shared_ptr<btcb::vote>, btcb::endpoint, bool = false);
	void verify_votes (std::deque<std::pair<std::shared_ptr<btcb::vote>, btcb::endpoint>> &);
	void flush ();
	void calculate_weights ();
	btcb::node & node;
	void stop ();

private:
	void process_loop ();
	std::deque<std::pair<std::shared_ptr<btcb::vote>, btcb::endpoint>> votes;
	// Representatives levels for random early detection
	std::unordered_set<btcb::account> representatives_1;
	std::unordered_set<btcb::account> representatives_2;
	std::unordered_set<btcb::account> representatives_3;
	std::condition_variable condition;
	std::mutex mutex;
	bool started;
	bool stopped;
	bool active;
	boost::thread thread;
};
// The network is crawled for representatives by occasionally sending a unicast confirm_req for a specific block and watching to see if it's acknowledged with a vote.
class rep_crawler
{
public:
	void add (btcb::block_hash const &);
	void remove (btcb::block_hash const &);
	bool exists (btcb::block_hash const &);
	std::mutex mutex;
	std::unordered_set<btcb::block_hash> active;
};
class block_processor;
class signature_check_set
{
public:
	size_t size;
	unsigned char const ** messages;
	size_t * message_lengths;
	unsigned char const ** pub_keys;
	unsigned char const ** signatures;
	int * verifications;
	std::promise<void> * promise;
};
class signature_checker
{
public:
	signature_checker ();
	~signature_checker ();
	void add (signature_check_set &);
	void stop ();
	void flush ();

private:
	void run ();
	void verify (btcb::signature_check_set & check_a);
	std::deque<btcb::signature_check_set> checks;
	bool started;
	bool stopped;
	std::mutex mutex;
	std::condition_variable condition;
	std::thread thread;
};
// Processing blocks is a potentially long IO operation
// This class isolates block insertion from other operations like servicing network operations
class block_processor
{
public:
	block_processor (btcb::node &);
	~block_processor ();
	void stop ();
	void flush ();
	bool full ();
	void add (std::shared_ptr<btcb::block>, std::chrono::steady_clock::time_point);
	void force (std::shared_ptr<btcb::block>);
	bool should_log (bool);
	bool have_blocks ();
	void process_blocks ();
	btcb::process_return process_receive_one (btcb::transaction const &, std::shared_ptr<btcb::block>, std::chrono::steady_clock::time_point = std::chrono::steady_clock::now (), bool = false);

private:
	void queue_unchecked (btcb::transaction const &, btcb::block_hash const &);
	void verify_state_blocks (std::unique_lock<std::mutex> &, size_t = std::numeric_limits<size_t>::max ());
	void process_receive_many (std::unique_lock<std::mutex> &);
	bool stopped;
	bool active;
	std::chrono::steady_clock::time_point next_log;
	std::deque<std::pair<std::shared_ptr<btcb::block>, std::chrono::steady_clock::time_point>> state_blocks;
	std::deque<std::pair<std::shared_ptr<btcb::block>, std::chrono::steady_clock::time_point>> blocks;
	std::unordered_set<btcb::block_hash> blocks_hashes;
	std::deque<std::shared_ptr<btcb::block>> forced;
	std::condition_variable condition;
	btcb::node & node;
	btcb::vote_generator generator;
	std::mutex mutex;
};
class node : public std::enable_shared_from_this<btcb::node>
{
public:
	node (btcb::node_init &, boost::asio::io_context &, uint16_t, boost::filesystem::path const &, btcb::alarm &, btcb::logging const &, btcb::work_pool &);
	node (btcb::node_init &, boost::asio::io_context &, boost::filesystem::path const &, btcb::alarm &, btcb::node_config const &, btcb::work_pool &);
	~node ();
	template <typename T>
	void background (T action_a)
	{
		alarm.io_ctx.post (action_a);
	}
	void send_keepalive (btcb::endpoint const &);
	bool copy_with_compaction (boost::filesystem::path const &);
	void keepalive (std::string const &, uint16_t);
	void start ();
	void stop ();
	std::shared_ptr<btcb::node> shared ();
	int store_version ();
	void process_confirmed (std::shared_ptr<btcb::block>);
	void process_message (btcb::message &, btcb::endpoint const &);
	void process_active (std::shared_ptr<btcb::block>);
	btcb::process_return process (btcb::block const &);
	void keepalive_preconfigured (std::vector<std::string> const &);
	btcb::block_hash latest (btcb::account const &);
	btcb::uint128_t balance (btcb::account const &);
	std::shared_ptr<btcb::block> block (btcb::block_hash const &);
	std::pair<btcb::uint128_t, btcb::uint128_t> balance_pending (btcb::account const &);
	btcb::uint128_t weight (btcb::account const &);
	btcb::account representative (btcb::account const &);
	void ongoing_keepalive ();
	void ongoing_syn_cookie_cleanup ();
	void ongoing_rep_crawl ();
	void ongoing_rep_calculation ();
	void ongoing_bootstrap ();
	void ongoing_store_flush ();
	void backup_wallet ();
	void search_pending ();
	int price (btcb::uint128_t const &, int);
	void work_generate_blocking (btcb::block &, uint64_t = btcb::work_pool::publish_threshold);
	uint64_t work_generate_blocking (btcb::uint256_union const &, uint64_t = btcb::work_pool::publish_threshold);
	void work_generate (btcb::uint256_union const &, std::function<void(uint64_t)>, uint64_t = btcb::work_pool::publish_threshold);
	void add_initial_peers ();
	void block_confirm (std::shared_ptr<btcb::block>);
	void process_fork (btcb::transaction const &, std::shared_ptr<btcb::block>);
	bool validate_block_by_previous (btcb::transaction const &, std::shared_ptr<btcb::block>);
	btcb::uint128_t delta ();
	boost::asio::io_context & io_ctx;
	btcb::node_config config;
	btcb::node_flags flags;
	btcb::alarm & alarm;
	btcb::work_pool & work;
	boost::log::sources::logger_mt log;
	std::unique_ptr<btcb::block_store> store_impl;
	btcb::block_store & store;
	btcb::gap_cache gap_cache;
	btcb::ledger ledger;
	btcb::active_transactions active;
	btcb::network network;
	btcb::bootstrap_initiator bootstrap_initiator;
	btcb::bootstrap_listener bootstrap;
	btcb::peer_container peers;
	boost::filesystem::path application_path;
	btcb::node_observers observers;
	btcb::wallets wallets;
	btcb::port_mapping port_mapping;
	btcb::signature_checker checker;
	btcb::vote_processor vote_processor;
	btcb::rep_crawler rep_crawler;
	unsigned warmed_up;
	btcb::block_processor block_processor;
	boost::thread block_processor_thread;
	btcb::block_arrival block_arrival;
	btcb::online_reps online_reps;
	btcb::stat stats;
	btcb::keypair node_id;
	btcb::block_uniquer block_uniquer;
	btcb::vote_uniquer vote_uniquer;
	static double constexpr price_max = 16.0;
	static double constexpr free_cutoff = 1024.0;
	static std::chrono::seconds constexpr period = std::chrono::seconds (60);
	static std::chrono::seconds constexpr cutoff = period * 5;
	static std::chrono::seconds constexpr syn_cookie_cutoff = std::chrono::seconds (5);
	static std::chrono::minutes constexpr backup_interval = std::chrono::minutes (5);
	static std::chrono::seconds constexpr search_pending_interval = (btcb::btcb_network == btcb::btcb_networks::btcb_test_network) ? std::chrono::seconds (1) : std::chrono::seconds (5 * 60);
};
class thread_runner
{
public:
	thread_runner (boost::asio::io_context &, unsigned);
	~thread_runner ();
	void join ();
	std::vector<boost::thread> threads;
};
class inactive_node
{
public:
	inactive_node (boost::filesystem::path const & path = btcb::working_path (), uint16_t = 24000);
	~inactive_node ();
	boost::filesystem::path path;
	std::shared_ptr<boost::asio::io_context> io_context;
	btcb::alarm alarm;
	btcb::logging logging;
	btcb::node_init init;
	btcb::work_pool work;
	uint16_t peering_port;
	std::shared_ptr<btcb::node> node;
};
}
