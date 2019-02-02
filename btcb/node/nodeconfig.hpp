#pragma once

#include <boost/property_tree/ptree.hpp>
#include <chrono>
#include <btcb/lib/numbers.hpp>
#include <btcb/node/logging.hpp>
#include <btcb/node/stats.hpp>
#include <vector>

namespace btcb
{
/**
 * Node configuration
 */
class node_config
{
public:
	node_config ();
	node_config (uint16_t, btcb::logging const &);
	void serialize_json (boost::property_tree::ptree &) const;
	bool deserialize_json (bool &, boost::property_tree::ptree &);
	bool upgrade_json (unsigned, boost::property_tree::ptree &);
	btcb::account random_representative ();
	uint16_t peering_port;
	btcb::logging logging;
	std::vector<std::pair<std::string, uint16_t>> work_peers;
	std::vector<std::string> preconfigured_peers;
	std::vector<btcb::account> preconfigured_representatives;
	unsigned bootstrap_fraction_numerator;
	btcb::amount receive_minimum;
	btcb::amount online_weight_minimum;
	unsigned online_weight_quorum;
	unsigned password_fanout;
	unsigned io_threads;
	unsigned network_threads;
	unsigned work_threads;
	bool enable_voting;
	unsigned bootstrap_connections;
	unsigned bootstrap_connections_max;
	std::string callback_address;
	uint16_t callback_port;
	std::string callback_target;
	int lmdb_max_dbs;
	bool allow_local_peers;
	btcb::stat_config stat_config;
	btcb::uint256_union epoch_block_link;
	btcb::account epoch_block_signer;
	std::chrono::milliseconds block_processor_batch_max_time;
	static std::chrono::seconds constexpr keepalive_period = std::chrono::seconds (60);
	static std::chrono::seconds constexpr keepalive_cutoff = keepalive_period * 5;
	static std::chrono::minutes constexpr wallet_backup_interval = std::chrono::minutes (5);
	static constexpr int json_version = 16;
};

class node_flags
{
public:
	node_flags ();
	bool disable_backup;
	bool disable_lazy_bootstrap;
	bool disable_legacy_bootstrap;
	bool disable_bootstrap_listener;
};
}
