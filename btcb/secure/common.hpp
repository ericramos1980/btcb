#pragma once

#include <btcb/lib/blockbuilders.hpp>
#include <btcb/lib/blocks.hpp>
#include <btcb/secure/utility.hpp>

#include <boost/iterator/transform_iterator.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/variant.hpp>

#include <unordered_map>

#include <blake2/blake2.h>

namespace boost
{
template <>
struct hash<::btcb::uint256_union>
{
	size_t operator() (::btcb::uint256_union const & value_a) const
	{
		std::hash<::btcb::uint256_union> hash;
		return hash (value_a);
	}
};
}
namespace btcb
{
const uint8_t protocol_version = 0x0f;
const uint8_t protocol_version_min = 0x0d;
const uint8_t node_id_version = 0x0c;

/*
 * Do not bootstrap from nodes older than this version.
 * Also, on the beta network do not process messages from
 * nodes older than this version.
 */
const uint8_t protocol_version_reasonable_min = 0x0d;

/**
 * A key pair. The private key is generated from the random pool, or passed in
 * as a hex string. The public key is derived using ed25519.
 */
class keypair
{
public:
	keypair ();
	keypair (std::string const &);
	keypair (btcb::raw_key &&);
	btcb::public_key pub;
	btcb::raw_key prv;
};

/**
 * Tag for which epoch an entry belongs to
 */
enum class epoch : uint8_t
{
	invalid = 0,
	unspecified = 1,
	epoch_0 = 2,
	epoch_1 = 3
};

/**
 * Latest information about an account
 */
class account_info
{
public:
	account_info ();
	account_info (btcb::account_info const &) = default;
	account_info (btcb::block_hash const &, btcb::block_hash const &, btcb::block_hash const &, btcb::amount const &, uint64_t, uint64_t, epoch);
	void serialize (btcb::stream &) const;
	bool deserialize (btcb::stream &);
	bool operator== (btcb::account_info const &) const;
	bool operator!= (btcb::account_info const &) const;
	size_t db_size () const;
	btcb::block_hash head;
	btcb::block_hash rep_block;
	btcb::block_hash open_block;
	btcb::amount balance;
	/** Seconds since posix epoch */
	uint64_t modified;
	uint64_t block_count;
	btcb::epoch epoch;
};

/**
 * Information on an uncollected send
 */
class pending_info
{
public:
	pending_info ();
	pending_info (btcb::account const &, btcb::amount const &, epoch);
	void serialize (btcb::stream &) const;
	bool deserialize (btcb::stream &);
	bool operator== (btcb::pending_info const &) const;
	btcb::account source;
	btcb::amount amount;
	btcb::epoch epoch;
};
class pending_key
{
public:
	pending_key ();
	pending_key (btcb::account const &, btcb::block_hash const &);
	void serialize (btcb::stream &) const;
	bool deserialize (btcb::stream &);
	bool operator== (btcb::pending_key const &) const;
	btcb::account account;
	btcb::block_hash hash;
	btcb::block_hash key () const;
};
// Internally unchecked_key is equal to pending_key (2x uint256_union)
using unchecked_key = pending_key;

class block_info
{
public:
	block_info ();
	block_info (btcb::account const &, btcb::amount const &);
	void serialize (btcb::stream &) const;
	bool deserialize (btcb::stream &);
	bool operator== (btcb::block_info const &) const;
	btcb::account account;
	btcb::amount balance;
};
class block_counts
{
public:
	block_counts ();
	size_t sum ();
	size_t send;
	size_t receive;
	size_t open;
	size_t change;
	size_t state_v0;
	size_t state_v1;
};
typedef std::vector<boost::variant<std::shared_ptr<btcb::block>, btcb::block_hash>>::const_iterator vote_blocks_vec_iter;
class iterate_vote_blocks_as_hash
{
public:
	iterate_vote_blocks_as_hash () = default;
	btcb::block_hash operator() (boost::variant<std::shared_ptr<btcb::block>, btcb::block_hash> const & item) const;
};
class vote
{
public:
	vote () = default;
	vote (btcb::vote const &);
	vote (bool &, btcb::stream &, btcb::block_uniquer * = nullptr);
	vote (bool &, btcb::stream &, btcb::block_type, btcb::block_uniquer * = nullptr);
	vote (btcb::account const &, btcb::raw_key const &, uint64_t, std::shared_ptr<btcb::block>);
	vote (btcb::account const &, btcb::raw_key const &, uint64_t, std::vector<btcb::block_hash>);
	std::string hashes_string () const;
	btcb::uint256_union hash () const;
	btcb::uint256_union full_hash () const;
	bool operator== (btcb::vote const &) const;
	bool operator!= (btcb::vote const &) const;
	void serialize (btcb::stream &, btcb::block_type);
	void serialize (btcb::stream &);
	bool deserialize (btcb::stream &, btcb::block_uniquer * = nullptr);
	bool validate ();
	boost::transform_iterator<btcb::iterate_vote_blocks_as_hash, btcb::vote_blocks_vec_iter> begin () const;
	boost::transform_iterator<btcb::iterate_vote_blocks_as_hash, btcb::vote_blocks_vec_iter> end () const;
	std::string to_json () const;
	// Vote round sequence number
	uint64_t sequence;
	// The blocks, or block hashes, that this vote is for
	std::vector<boost::variant<std::shared_ptr<btcb::block>, btcb::block_hash>> blocks;
	// Account that's voting
	btcb::account account;
	// Signature of sequence + block hashes
	btcb::signature signature;
	static const std::string hash_prefix;
};
/**
 * This class serves to find and return unique variants of a vote in order to minimize memory usage
 */
class vote_uniquer
{
public:
	vote_uniquer (btcb::block_uniquer &);
	std::shared_ptr<btcb::vote> unique (std::shared_ptr<btcb::vote>);
	size_t size ();

private:
	btcb::block_uniquer & uniquer;
	std::mutex mutex;
	std::unordered_map<btcb::uint256_union, std::weak_ptr<btcb::vote>> votes;
	static unsigned constexpr cleanup_count = 2;
};
enum class vote_code
{
	invalid, // Vote is not signed correctly
	replay, // Vote does not have the highest sequence number, it's a replay
	vote // Vote has the highest sequence number
};

enum class process_result
{
	progress, // Hasn't been seen before, signed correctly
	bad_signature, // Signature was bad, forged or transmission error
	old, // Already seen and was valid
	negative_spend, // Malicious attempt to spend a negative amount
	fork, // Malicious fork based on previous
	unreceivable, // Source block doesn't exist, has already been received, or requires an account upgrade (epoch blocks)
	gap_previous, // Block marked as previous is unknown
	gap_source, // Block marked as source is unknown
	opened_burn_account, // The impossible happened, someone found the private key associated with the public key '0'.
	balance_mismatch, // Balance and amount delta don't match
	representative_mismatch, // Representative is changed when it is not allowed
	block_position // This block cannot follow the previous block
};
class process_return
{
public:
	btcb::process_result code;
	btcb::account account;
	btcb::amount amount;
	btcb::account pending_account;
	boost::optional<bool> state_is_send;
};
enum class tally_result
{
	vote,
	changed,
	confirm
};
extern btcb::keypair const & zero_key;
extern btcb::keypair const & test_genesis_key;
extern btcb::account const & btcb_test_account;
extern btcb::account const & btcb_beta_account;
extern btcb::account const & btcb_live_account;
extern std::string const & btcb_test_genesis;
extern std::string const & btcb_beta_genesis;
extern std::string const & btcb_live_genesis;
extern std::string const & genesis_block;
extern btcb::account const & genesis_account;
extern btcb::account const & burn_account;
extern btcb::uint128_t const & genesis_amount;
// A block hash that compares inequal to any real block hash
extern btcb::block_hash const & not_a_block;
// An account number that compares inequal to any real account number
extern btcb::block_hash const & not_an_account;
class genesis
{
public:
	explicit genesis ();
	btcb::block_hash hash () const;
	std::shared_ptr<btcb::block> open;
};
}
