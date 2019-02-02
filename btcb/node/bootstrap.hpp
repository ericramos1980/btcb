#pragma once

#include <btcb/node/common.hpp>
#include <btcb/secure/blockstore.hpp>
#include <btcb/secure/ledger.hpp>

#include <atomic>
#include <future>
#include <queue>
#include <stack>
#include <unordered_set>

#include <boost/log/sources/logger.hpp>
#include <boost/thread/thread.hpp>

namespace btcb
{
class bootstrap_attempt;
class bootstrap_client;
class node;
enum class sync_result
{
	success,
	error,
	fork
};
class socket : public std::enable_shared_from_this<btcb::socket>
{
public:
	socket (std::shared_ptr<btcb::node>);
	void async_connect (btcb::tcp_endpoint const &, std::function<void(boost::system::error_code const &)>);
	void async_read (std::shared_ptr<std::vector<uint8_t>>, size_t, std::function<void(boost::system::error_code const &, size_t)>);
	void async_write (std::shared_ptr<std::vector<uint8_t>>, std::function<void(boost::system::error_code const &, size_t)>);
	void start (std::chrono::steady_clock::time_point = std::chrono::steady_clock::now () + std::chrono::seconds (5));
	void stop ();
	void close ();
	void checkup ();
	btcb::tcp_endpoint remote_endpoint ();
	boost::asio::ip::tcp::socket socket_m;

private:
	std::atomic<uint64_t> cutoff;
	std::shared_ptr<btcb::node> node;
};

/**
 * The length of every message header, parsed by btcb::message::read_header ()
 * The 2 here represents the size of a std::bitset<16>, which is 2 chars long normally
 */
static const int bootstrap_message_header_size = sizeof (btcb::message_header::magic_number) + sizeof (uint8_t) + sizeof (uint8_t) + sizeof (uint8_t) + sizeof (btcb::message_type) + 2;

class bootstrap_client;
class pull_info
{
public:
	typedef btcb::bulk_pull::count_t count_t;
	pull_info ();
	pull_info (btcb::account const &, btcb::block_hash const &, btcb::block_hash const &, count_t = 0);
	btcb::account account;
	btcb::block_hash head;
	btcb::block_hash end;
	count_t count;
	unsigned attempts;
};
class frontier_req_client;
class bulk_push_client;
class bootstrap_attempt : public std::enable_shared_from_this<bootstrap_attempt>
{
public:
	bootstrap_attempt (std::shared_ptr<btcb::node> node_a);
	~bootstrap_attempt ();
	void run ();
	std::shared_ptr<btcb::bootstrap_client> connection (std::unique_lock<std::mutex> &);
	bool consume_future (std::future<bool> &);
	void populate_connections ();
	bool request_frontier (std::unique_lock<std::mutex> &);
	void request_pull (std::unique_lock<std::mutex> &);
	void request_push (std::unique_lock<std::mutex> &);
	void add_connection (btcb::endpoint const &);
	void pool_connection (std::shared_ptr<btcb::bootstrap_client>);
	void stop ();
	void requeue_pull (btcb::pull_info const &);
	void add_pull (btcb::pull_info const &);
	bool still_pulling ();
	unsigned target_connections (size_t pulls_remaining);
	bool should_log ();
	void add_bulk_push_target (btcb::block_hash const &, btcb::block_hash const &);
	bool process_block (std::shared_ptr<btcb::block>, uint64_t, bool);
	void lazy_run ();
	void lazy_start (btcb::block_hash const &);
	void lazy_add (btcb::block_hash const &);
	bool lazy_finished ();
	void lazy_pull_flush ();
	std::chrono::steady_clock::time_point next_log;
	std::deque<std::weak_ptr<btcb::bootstrap_client>> clients;
	std::weak_ptr<btcb::bootstrap_client> connection_frontier_request;
	std::weak_ptr<btcb::frontier_req_client> frontiers;
	std::weak_ptr<btcb::bulk_push_client> push;
	std::deque<btcb::pull_info> pulls;
	std::deque<std::shared_ptr<btcb::bootstrap_client>> idle;
	std::atomic<unsigned> connections;
	std::atomic<unsigned> pulling;
	std::shared_ptr<btcb::node> node;
	std::atomic<unsigned> account_count;
	std::atomic<uint64_t> total_blocks;
	std::vector<std::pair<btcb::block_hash, btcb::block_hash>> bulk_push_targets;
	bool stopped;
	bool lazy_mode;
	std::mutex mutex;
	std::condition_variable condition;
	// Lazy bootstrap
	std::unordered_set<btcb::block_hash> lazy_blocks;
	std::unordered_map<btcb::block_hash, std::pair<btcb::block_hash, btcb::uint128_t>> lazy_state_unknown;
	std::unordered_map<btcb::block_hash, btcb::uint128_t> lazy_balances;
	std::unordered_set<btcb::block_hash> lazy_keys;
	std::deque<btcb::block_hash> lazy_pulls;
	std::atomic<uint64_t> lazy_stopped;
	uint64_t lazy_max_pull_blocks = (btcb::btcb_network == btcb::btcb_networks::btcb_test_network) ? 2 : 512;
	uint64_t lazy_max_stopped = 256;
	std::mutex lazy_mutex;
};
class frontier_req_client : public std::enable_shared_from_this<btcb::frontier_req_client>
{
public:
	frontier_req_client (std::shared_ptr<btcb::bootstrap_client>);
	~frontier_req_client ();
	void run ();
	void receive_frontier ();
	void received_frontier (boost::system::error_code const &, size_t);
	void request_account (btcb::account const &, btcb::block_hash const &);
	void unsynced (btcb::block_hash const &, btcb::block_hash const &);
	void next (btcb::transaction const &);
	void insert_pull (btcb::pull_info const &);
	std::shared_ptr<btcb::bootstrap_client> connection;
	btcb::account current;
	btcb::block_hash frontier;
	unsigned count;
	btcb::account landing;
	btcb::account faucet;
	std::chrono::steady_clock::time_point start_time;
	std::promise<bool> promise;
	/** A very rough estimate of the cost of `bulk_push`ing missing blocks */
	uint64_t bulk_push_cost;
	std::deque<std::pair<btcb::account, btcb::block_hash>> accounts;
};
class bulk_pull_client : public std::enable_shared_from_this<btcb::bulk_pull_client>
{
public:
	bulk_pull_client (std::shared_ptr<btcb::bootstrap_client>, btcb::pull_info const &);
	~bulk_pull_client ();
	void request ();
	void receive_block ();
	void received_type ();
	void received_block (boost::system::error_code const &, size_t, btcb::block_type);
	btcb::block_hash first ();
	std::shared_ptr<btcb::bootstrap_client> connection;
	btcb::block_hash expected;
	btcb::pull_info pull;
	uint64_t total_blocks;
};
class bootstrap_client : public std::enable_shared_from_this<bootstrap_client>
{
public:
	bootstrap_client (std::shared_ptr<btcb::node>, std::shared_ptr<btcb::bootstrap_attempt>, btcb::tcp_endpoint const &);
	~bootstrap_client ();
	void run ();
	std::shared_ptr<btcb::bootstrap_client> shared ();
	void stop (bool force);
	double block_rate () const;
	double elapsed_seconds () const;
	std::shared_ptr<btcb::node> node;
	std::shared_ptr<btcb::bootstrap_attempt> attempt;
	std::shared_ptr<btcb::socket> socket;
	std::shared_ptr<std::vector<uint8_t>> receive_buffer;
	btcb::tcp_endpoint endpoint;
	std::chrono::steady_clock::time_point start_time;
	std::atomic<uint64_t> block_count;
	std::atomic<bool> pending_stop;
	std::atomic<bool> hard_stop;
};
class bulk_push_client : public std::enable_shared_from_this<btcb::bulk_push_client>
{
public:
	bulk_push_client (std::shared_ptr<btcb::bootstrap_client> const &);
	~bulk_push_client ();
	void start ();
	void push (btcb::transaction const &);
	void push_block (btcb::block const &);
	void send_finished ();
	std::shared_ptr<btcb::bootstrap_client> connection;
	std::promise<bool> promise;
	std::pair<btcb::block_hash, btcb::block_hash> current_target;
};
class bootstrap_initiator
{
public:
	bootstrap_initiator (btcb::node &);
	~bootstrap_initiator ();
	void bootstrap (btcb::endpoint const &, bool add_to_peers = true);
	void bootstrap ();
	void bootstrap_lazy (btcb::block_hash const &, bool = false);
	void run_bootstrap ();
	void notify_listeners (bool);
	void add_observer (std::function<void(bool)> const &);
	bool in_progress ();
	std::shared_ptr<btcb::bootstrap_attempt> current_attempt ();
	void stop ();

private:
	btcb::node & node;
	std::shared_ptr<btcb::bootstrap_attempt> attempt;
	bool stopped;
	std::mutex mutex;
	std::condition_variable condition;
	std::vector<std::function<void(bool)>> observers;
	boost::thread thread;
};
class bootstrap_server;
class bootstrap_listener
{
public:
	bootstrap_listener (boost::asio::io_context &, uint16_t, btcb::node &);
	void start ();
	void stop ();
	void accept_connection ();
	void accept_action (boost::system::error_code const &, std::shared_ptr<btcb::socket>);
	std::mutex mutex;
	std::unordered_map<btcb::bootstrap_server *, std::weak_ptr<btcb::bootstrap_server>> connections;
	btcb::tcp_endpoint endpoint ();
	boost::asio::ip::tcp::acceptor acceptor;
	btcb::tcp_endpoint local;
	boost::asio::io_context & io_ctx;
	btcb::node & node;
	bool on;
};
class message;
class bootstrap_server : public std::enable_shared_from_this<btcb::bootstrap_server>
{
public:
	bootstrap_server (std::shared_ptr<btcb::socket>, std::shared_ptr<btcb::node>);
	~bootstrap_server ();
	void receive ();
	void receive_header_action (boost::system::error_code const &, size_t);
	void receive_bulk_pull_action (boost::system::error_code const &, size_t, btcb::message_header const &);
	void receive_bulk_pull_account_action (boost::system::error_code const &, size_t, btcb::message_header const &);
	void receive_bulk_pull_blocks_action (boost::system::error_code const &, size_t, btcb::message_header const &);
	void receive_frontier_req_action (boost::system::error_code const &, size_t, btcb::message_header const &);
	void receive_bulk_push_action ();
	void add_request (std::unique_ptr<btcb::message>);
	void finish_request ();
	void run_next ();
	std::shared_ptr<std::vector<uint8_t>> receive_buffer;
	std::shared_ptr<btcb::socket> socket;
	std::shared_ptr<btcb::node> node;
	std::mutex mutex;
	std::queue<std::unique_ptr<btcb::message>> requests;
};
class bulk_pull;
class bulk_pull_server : public std::enable_shared_from_this<btcb::bulk_pull_server>
{
public:
	bulk_pull_server (std::shared_ptr<btcb::bootstrap_server> const &, std::unique_ptr<btcb::bulk_pull>);
	void set_current_end ();
	std::shared_ptr<btcb::block> get_next ();
	void send_next ();
	void sent_action (boost::system::error_code const &, size_t);
	void send_finished ();
	void no_block_sent (boost::system::error_code const &, size_t);
	std::shared_ptr<btcb::bootstrap_server> connection;
	std::unique_ptr<btcb::bulk_pull> request;
	std::shared_ptr<std::vector<uint8_t>> send_buffer;
	btcb::block_hash current;
	bool include_start;
	btcb::bulk_pull::count_t max_count;
	btcb::bulk_pull::count_t sent_count;
};
class bulk_pull_account;
class bulk_pull_account_server : public std::enable_shared_from_this<btcb::bulk_pull_account_server>
{
public:
	bulk_pull_account_server (std::shared_ptr<btcb::bootstrap_server> const &, std::unique_ptr<btcb::bulk_pull_account>);
	void set_params ();
	std::pair<std::unique_ptr<btcb::pending_key>, std::unique_ptr<btcb::pending_info>> get_next ();
	void send_frontier ();
	void send_next_block ();
	void sent_action (boost::system::error_code const &, size_t);
	void send_finished ();
	void complete (boost::system::error_code const &, size_t);
	std::shared_ptr<btcb::bootstrap_server> connection;
	std::unique_ptr<btcb::bulk_pull_account> request;
	std::shared_ptr<std::vector<uint8_t>> send_buffer;
	std::unordered_map<btcb::uint256_union, bool> deduplication;
	btcb::pending_key current_key;
	bool pending_address_only;
	bool pending_include_address;
	bool invalid_request;
};
class bulk_pull_blocks;
class bulk_pull_blocks_server : public std::enable_shared_from_this<btcb::bulk_pull_blocks_server>
{
public:
	bulk_pull_blocks_server (std::shared_ptr<btcb::bootstrap_server> const &, std::unique_ptr<btcb::bulk_pull_blocks>);
	void set_params ();
	std::shared_ptr<btcb::block> get_next ();
	void send_next ();
	void send_finished ();
	void no_block_sent (boost::system::error_code const &, size_t);
	std::shared_ptr<btcb::bootstrap_server> connection;
	std::unique_ptr<btcb::bulk_pull_blocks> request;
	std::shared_ptr<std::vector<uint8_t>> send_buffer;
};
class bulk_push_server : public std::enable_shared_from_this<btcb::bulk_push_server>
{
public:
	bulk_push_server (std::shared_ptr<btcb::bootstrap_server> const &);
	void receive ();
	void receive_block ();
	void received_type ();
	void received_block (boost::system::error_code const &, size_t, btcb::block_type);
	std::shared_ptr<std::vector<uint8_t>> receive_buffer;
	std::shared_ptr<btcb::bootstrap_server> connection;
};
class frontier_req;
class frontier_req_server : public std::enable_shared_from_this<btcb::frontier_req_server>
{
public:
	frontier_req_server (std::shared_ptr<btcb::bootstrap_server> const &, std::unique_ptr<btcb::frontier_req>);
	void send_next ();
	void sent_action (boost::system::error_code const &, size_t);
	void send_finished ();
	void no_block_sent (boost::system::error_code const &, size_t);
	void next ();
	std::shared_ptr<btcb::bootstrap_server> connection;
	btcb::account current;
	btcb::block_hash frontier;
	std::unique_ptr<btcb::frontier_req> request;
	std::shared_ptr<std::vector<uint8_t>> send_buffer;
	size_t count;
	std::deque<std::pair<btcb::account, btcb::block_hash>> accounts;
};
}
