#include <btcb/secure/common.hpp>

#include <btcb/lib/interface.h>
#include <btcb/lib/numbers.hpp>
#include <btcb/node/common.hpp>
#include <btcb/node/btcb.hpp>
#include <btcb/secure/blockstore.hpp>
#include <btcb/secure/versioning.hpp>

#include <boost/property_tree/json_parser.hpp>

#include <queue>

#include <ed25519-donna/ed25519.h>

// Genesis keys for network variants
namespace
{
char const * test_private_key_data = "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4";
char const * test_public_key_data = "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // bcb_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo
char const * beta_public_key_data = btcb::bootstrap::BETA_GENESIS_PUBK;
char const * live_public_key_data = btcb::bootstrap::LIVE_GENESIS_PUBK;
char const * test_genesis_data = R"%%%({
	"type": "open",
	"source": "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
	"representative": "bcb_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
	"account": "bcb_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
	"work": "9680625b39d3363d",
	"signature": "ECDA914373A2F0CA1296475BAEE40500A7F0A7AD72A5A80C81D7FAB7F6C802B2CC7DB50F5DD0FB25B2EF11761FA7344A158DD5A700B21BD47DE5BD0F63153A02"
})%%%";

char const * beta_genesis_data = btcb::bootstrap::BETA_GENESIS_BLOCK;

char const * live_genesis_data = btcb::bootstrap::LIVE_GENESIS_BLOCK;

class ledger_constants
{
public:
	ledger_constants () :
	zero_key ("0"),
	test_genesis_key (test_private_key_data),
	btcb_test_account (test_public_key_data),
	btcb_beta_account (beta_public_key_data),
	btcb_live_account (live_public_key_data),
	btcb_test_genesis (test_genesis_data),
	btcb_beta_genesis (beta_genesis_data),
	btcb_live_genesis (live_genesis_data),
	genesis_account (btcb::btcb_network == btcb::btcb_networks::btcb_test_network ? btcb_test_account : btcb::btcb_network == btcb::btcb_networks::btcb_beta_network ? btcb_beta_account : btcb_live_account),
	genesis_block (btcb::btcb_network == btcb::btcb_networks::btcb_test_network ? btcb_test_genesis : btcb::btcb_network == btcb::btcb_networks::btcb_beta_network ? btcb_beta_genesis : btcb_live_genesis),
	genesis_amount (std::numeric_limits<btcb::uint128_t>::max ()),
	burn_account (0)
	{
		CryptoPP::AutoSeededRandomPool random_pool;
		// Randomly generating these mean no two nodes will ever have the same sentinel values which protects against some insecure algorithms
		random_pool.GenerateBlock (not_a_block.bytes.data (), not_a_block.bytes.size ());
		random_pool.GenerateBlock (not_an_account.bytes.data (), not_an_account.bytes.size ());
	}
	btcb::keypair zero_key;
	btcb::keypair test_genesis_key;
	btcb::account btcb_test_account;
	btcb::account btcb_beta_account;
	btcb::account btcb_live_account;
	std::string btcb_test_genesis;
	std::string btcb_beta_genesis;
	std::string btcb_live_genesis;
	btcb::account genesis_account;
	std::string genesis_block;
	btcb::uint128_t genesis_amount;
	btcb::block_hash not_a_block;
	btcb::account not_an_account;
	btcb::account burn_account;
};
ledger_constants globals;
}

size_t constexpr btcb::send_block::size;
size_t constexpr btcb::receive_block::size;
size_t constexpr btcb::open_block::size;
size_t constexpr btcb::change_block::size;
size_t constexpr btcb::state_block::size;

btcb::keypair const & btcb::zero_key (globals.zero_key);
btcb::keypair const & btcb::test_genesis_key (globals.test_genesis_key);
btcb::account const & btcb::btcb_test_account (globals.btcb_test_account);
btcb::account const & btcb::btcb_beta_account (globals.btcb_beta_account);
btcb::account const & btcb::btcb_live_account (globals.btcb_live_account);
std::string const & btcb::btcb_test_genesis (globals.btcb_test_genesis);
std::string const & btcb::btcb_beta_genesis (globals.btcb_beta_genesis);
std::string const & btcb::btcb_live_genesis (globals.btcb_live_genesis);

btcb::account const & btcb::genesis_account (globals.genesis_account);
std::string const & btcb::genesis_block (globals.genesis_block);
btcb::uint128_t const & btcb::genesis_amount (globals.genesis_amount);
btcb::block_hash const & btcb::not_a_block (globals.not_a_block);
btcb::block_hash const & btcb::not_an_account (globals.not_an_account);
btcb::account const & btcb::burn_account (globals.burn_account);

// Create a new random keypair
btcb::keypair::keypair ()
{
	random_pool.GenerateBlock (prv.data.bytes.data (), prv.data.bytes.size ());
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Create a keypair given a private key
btcb::keypair::keypair (btcb::raw_key && prv_a) :
prv (std::move (prv_a))
{
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Create a keypair given a hex string of the private key
btcb::keypair::keypair (std::string const & prv_a)
{
	auto error (prv.data.decode_hex (prv_a));
	assert (!error);
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Serialize a block prefixed with an 8-bit typecode
void btcb::serialize_block (btcb::stream & stream_a, btcb::block const & block_a)
{
	write (stream_a, block_a.type ());
	block_a.serialize (stream_a);
}

btcb::account_info::account_info () :
head (0),
rep_block (0),
open_block (0),
balance (0),
modified (0),
block_count (0),
epoch (btcb::epoch::epoch_0)
{
}

btcb::account_info::account_info (btcb::block_hash const & head_a, btcb::block_hash const & rep_block_a, btcb::block_hash const & open_block_a, btcb::amount const & balance_a, uint64_t modified_a, uint64_t block_count_a, btcb::epoch epoch_a) :
head (head_a),
rep_block (rep_block_a),
open_block (open_block_a),
balance (balance_a),
modified (modified_a),
block_count (block_count_a),
epoch (epoch_a)
{
}

void btcb::account_info::serialize (btcb::stream & stream_a) const
{
	write (stream_a, head.bytes);
	write (stream_a, rep_block.bytes);
	write (stream_a, open_block.bytes);
	write (stream_a, balance.bytes);
	write (stream_a, modified);
	write (stream_a, block_count);
}

bool btcb::account_info::deserialize (btcb::stream & stream_a)
{
	auto error (read (stream_a, head.bytes));
	if (!error)
	{
		error = read (stream_a, rep_block.bytes);
		if (!error)
		{
			error = read (stream_a, open_block.bytes);
			if (!error)
			{
				error = read (stream_a, balance.bytes);
				if (!error)
				{
					error = read (stream_a, modified);
					if (!error)
					{
						error = read (stream_a, block_count);
					}
				}
			}
		}
	}
	return error;
}

bool btcb::account_info::operator== (btcb::account_info const & other_a) const
{
	return head == other_a.head && rep_block == other_a.rep_block && open_block == other_a.open_block && balance == other_a.balance && modified == other_a.modified && block_count == other_a.block_count && epoch == other_a.epoch;
}

bool btcb::account_info::operator!= (btcb::account_info const & other_a) const
{
	return !(*this == other_a);
}

size_t btcb::account_info::db_size () const
{
	assert (reinterpret_cast<const uint8_t *> (this) == reinterpret_cast<const uint8_t *> (&head));
	assert (reinterpret_cast<const uint8_t *> (&head) + sizeof (head) == reinterpret_cast<const uint8_t *> (&rep_block));
	assert (reinterpret_cast<const uint8_t *> (&rep_block) + sizeof (rep_block) == reinterpret_cast<const uint8_t *> (&open_block));
	assert (reinterpret_cast<const uint8_t *> (&open_block) + sizeof (open_block) == reinterpret_cast<const uint8_t *> (&balance));
	assert (reinterpret_cast<const uint8_t *> (&balance) + sizeof (balance) == reinterpret_cast<const uint8_t *> (&modified));
	assert (reinterpret_cast<const uint8_t *> (&modified) + sizeof (modified) == reinterpret_cast<const uint8_t *> (&block_count));
	return sizeof (head) + sizeof (rep_block) + sizeof (open_block) + sizeof (balance) + sizeof (modified) + sizeof (block_count);
}

btcb::block_counts::block_counts () :
send (0),
receive (0),
open (0),
change (0),
state_v0 (0),
state_v1 (0)
{
}

size_t btcb::block_counts::sum ()
{
	return send + receive + open + change + state_v0 + state_v1;
}

btcb::pending_info::pending_info () :
source (0),
amount (0),
epoch (btcb::epoch::epoch_0)
{
}

btcb::pending_info::pending_info (btcb::account const & source_a, btcb::amount const & amount_a, btcb::epoch epoch_a) :
source (source_a),
amount (amount_a),
epoch (epoch_a)
{
}

void btcb::pending_info::serialize (btcb::stream & stream_a) const
{
	btcb::write (stream_a, source.bytes);
	btcb::write (stream_a, amount.bytes);
}

bool btcb::pending_info::deserialize (btcb::stream & stream_a)
{
	auto result (btcb::read (stream_a, source.bytes));
	if (!result)
	{
		result = btcb::read (stream_a, amount.bytes);
	}
	return result;
}

bool btcb::pending_info::operator== (btcb::pending_info const & other_a) const
{
	return source == other_a.source && amount == other_a.amount && epoch == other_a.epoch;
}

btcb::pending_key::pending_key () :
account (0),
hash (0)
{
}

btcb::pending_key::pending_key (btcb::account const & account_a, btcb::block_hash const & hash_a) :
account (account_a),
hash (hash_a)
{
}

void btcb::pending_key::serialize (btcb::stream & stream_a) const
{
	btcb::write (stream_a, account.bytes);
	btcb::write (stream_a, hash.bytes);
}

bool btcb::pending_key::deserialize (btcb::stream & stream_a)
{
	auto error (btcb::read (stream_a, account.bytes));
	if (!error)
	{
		error = btcb::read (stream_a, hash.bytes);
	}
	return error;
}

bool btcb::pending_key::operator== (btcb::pending_key const & other_a) const
{
	return account == other_a.account && hash == other_a.hash;
}

btcb::block_hash btcb::pending_key::key () const
{
	return account;
}

btcb::block_info::block_info () :
account (0),
balance (0)
{
}

btcb::block_info::block_info (btcb::account const & account_a, btcb::amount const & balance_a) :
account (account_a),
balance (balance_a)
{
}

void btcb::block_info::serialize (btcb::stream & stream_a) const
{
	btcb::write (stream_a, account.bytes);
	btcb::write (stream_a, balance.bytes);
}

bool btcb::block_info::deserialize (btcb::stream & stream_a)
{
	auto error (btcb::read (stream_a, account.bytes));
	if (!error)
	{
		error = btcb::read (stream_a, balance.bytes);
	}
	return error;
}

bool btcb::block_info::operator== (btcb::block_info const & other_a) const
{
	return account == other_a.account && balance == other_a.balance;
}

bool btcb::vote::operator== (btcb::vote const & other_a) const
{
	auto blocks_equal (true);
	if (blocks.size () != other_a.blocks.size ())
	{
		blocks_equal = false;
	}
	else
	{
		for (auto i (0); blocks_equal && i < blocks.size (); ++i)
		{
			auto block (blocks[i]);
			auto other_block (other_a.blocks[i]);
			if (block.which () != other_block.which ())
			{
				blocks_equal = false;
			}
			else if (block.which ())
			{
				if (boost::get<btcb::block_hash> (block) != boost::get<btcb::block_hash> (other_block))
				{
					blocks_equal = false;
				}
			}
			else
			{
				if (!(*boost::get<std::shared_ptr<btcb::block>> (block) == *boost::get<std::shared_ptr<btcb::block>> (other_block)))
				{
					blocks_equal = false;
				}
			}
		}
	}
	return sequence == other_a.sequence && blocks_equal && account == other_a.account && signature == other_a.signature;
}

bool btcb::vote::operator!= (btcb::vote const & other_a) const
{
	return !(*this == other_a);
}

std::string btcb::vote::to_json () const
{
	std::stringstream stream;
	boost::property_tree::ptree tree;
	tree.put ("account", account.to_account ());
	tree.put ("signature", signature.number ());
	tree.put ("sequence", std::to_string (sequence));
	boost::property_tree::ptree blocks_tree;
	for (auto block : blocks)
	{
		if (block.which ())
		{
			blocks_tree.put ("", boost::get<std::shared_ptr<btcb::block>> (block)->to_json ());
		}
		else
		{
			blocks_tree.put ("", boost::get<std::shared_ptr<btcb::block>> (block)->hash ().to_string ());
		}
	}
	tree.add_child ("blocks", blocks_tree);
	boost::property_tree::write_json (stream, tree);
	return stream.str ();
}

btcb::vote::vote (btcb::vote const & other_a) :
sequence (other_a.sequence),
blocks (other_a.blocks),
account (other_a.account),
signature (other_a.signature)
{
}

btcb::vote::vote (bool & error_a, btcb::stream & stream_a, btcb::block_uniquer * uniquer_a)
{
	error_a = deserialize (stream_a, uniquer_a);
}

btcb::vote::vote (bool & error_a, btcb::stream & stream_a, btcb::block_type type_a, btcb::block_uniquer * uniquer_a)
{
	if (!error_a)
	{
		error_a = btcb::read (stream_a, account.bytes);
		if (!error_a)
		{
			error_a = btcb::read (stream_a, signature.bytes);
			if (!error_a)
			{
				error_a = btcb::read (stream_a, sequence);
				if (!error_a)
				{
					while (!error_a && stream_a.in_avail () > 0)
					{
						if (type_a == btcb::block_type::not_a_block)
						{
							btcb::block_hash block_hash;
							error_a = btcb::read (stream_a, block_hash);
							if (!error_a)
							{
								blocks.push_back (block_hash);
							}
						}
						else
						{
							std::shared_ptr<btcb::block> block (btcb::deserialize_block (stream_a, type_a, uniquer_a));
							error_a = block == nullptr;
							if (!error_a)
							{
								blocks.push_back (block);
							}
						}
					}
					if (blocks.empty ())
					{
						error_a = true;
					}
				}
			}
		}
	}
}

btcb::vote::vote (btcb::account const & account_a, btcb::raw_key const & prv_a, uint64_t sequence_a, std::shared_ptr<btcb::block> block_a) :
sequence (sequence_a),
blocks (1, block_a),
account (account_a),
signature (btcb::sign_message (prv_a, account_a, hash ()))
{
}

btcb::vote::vote (btcb::account const & account_a, btcb::raw_key const & prv_a, uint64_t sequence_a, std::vector<btcb::block_hash> blocks_a) :
sequence (sequence_a),
account (account_a)
{
	assert (blocks_a.size () > 0);
	assert (blocks_a.size () <= 12);
	for (auto hash : blocks_a)
	{
		blocks.push_back (hash);
	}
	signature = btcb::sign_message (prv_a, account_a, hash ());
}

std::string btcb::vote::hashes_string () const
{
	std::string result;
	for (auto hash : *this)
	{
		result += hash.to_string ();
		result += ", ";
	}
	return result;
}

const std::string btcb::vote::hash_prefix = "vote ";

btcb::uint256_union btcb::vote::hash () const
{
	btcb::uint256_union result;
	blake2b_state hash;
	blake2b_init (&hash, sizeof (result.bytes));
	if (blocks.size () > 1 || (blocks.size () > 0 && blocks[0].which ()))
	{
		blake2b_update (&hash, hash_prefix.data (), hash_prefix.size ());
	}
	for (auto block_hash : *this)
	{
		blake2b_update (&hash, block_hash.bytes.data (), sizeof (block_hash.bytes));
	}
	union
	{
		uint64_t qword;
		std::array<uint8_t, 8> bytes;
	};
	qword = sequence;
	blake2b_update (&hash, bytes.data (), sizeof (bytes));
	blake2b_final (&hash, result.bytes.data (), sizeof (result.bytes));
	return result;
}

btcb::uint256_union btcb::vote::full_hash () const
{
	btcb::uint256_union result;
	blake2b_state state;
	blake2b_init (&state, sizeof (result.bytes));
	blake2b_update (&state, hash ().bytes.data (), sizeof (hash ().bytes));
	blake2b_update (&state, account.bytes.data (), sizeof (account.bytes.data ()));
	blake2b_update (&state, signature.bytes.data (), sizeof (signature.bytes.data ()));
	blake2b_final (&state, result.bytes.data (), sizeof (result.bytes));
	return result;
}

void btcb::vote::serialize (btcb::stream & stream_a, btcb::block_type type)
{
	write (stream_a, account);
	write (stream_a, signature);
	write (stream_a, sequence);
	for (auto block : blocks)
	{
		if (block.which ())
		{
			assert (type == btcb::block_type::not_a_block);
			write (stream_a, boost::get<btcb::block_hash> (block));
		}
		else
		{
			if (type == btcb::block_type::not_a_block)
			{
				write (stream_a, boost::get<std::shared_ptr<btcb::block>> (block)->hash ());
			}
			else
			{
				boost::get<std::shared_ptr<btcb::block>> (block)->serialize (stream_a);
			}
		}
	}
}

void btcb::vote::serialize (btcb::stream & stream_a)
{
	write (stream_a, account);
	write (stream_a, signature);
	write (stream_a, sequence);
	for (auto block : blocks)
	{
		if (block.which ())
		{
			write (stream_a, btcb::block_type::not_a_block);
			write (stream_a, boost::get<btcb::block_hash> (block));
		}
		else
		{
			btcb::serialize_block (stream_a, *boost::get<std::shared_ptr<btcb::block>> (block));
		}
	}
}

bool btcb::vote::deserialize (btcb::stream & stream_a, btcb::block_uniquer * uniquer_a)
{
	auto result (read (stream_a, account));
	if (!result)
	{
		result = read (stream_a, signature);
		if (!result)
		{
			result = read (stream_a, sequence);
			if (!result)
			{
				btcb::block_type type;
				while (!result)
				{
					if (btcb::read (stream_a, type))
					{
						if (blocks.empty ())
						{
							result = true;
						}
						break;
					}
					if (!result)
					{
						if (type == btcb::block_type::not_a_block)
						{
							btcb::block_hash block_hash;
							result = btcb::read (stream_a, block_hash);
							if (!result)
							{
								blocks.push_back (block_hash);
							}
						}
						else
						{
							std::shared_ptr<btcb::block> block (btcb::deserialize_block (stream_a, type, uniquer_a));
							result = block == nullptr;
							if (!result)
							{
								blocks.push_back (block);
							}
						}
					}
				}
			}
		}
	}
	return result;
}

bool btcb::vote::validate ()
{
	auto result (btcb::validate_message (account, hash (), signature));
	return result;
}

btcb::block_hash btcb::iterate_vote_blocks_as_hash::operator() (boost::variant<std::shared_ptr<btcb::block>, btcb::block_hash> const & item) const
{
	btcb::block_hash result;
	if (item.which ())
	{
		result = boost::get<btcb::block_hash> (item);
	}
	else
	{
		result = boost::get<std::shared_ptr<btcb::block>> (item)->hash ();
	}
	return result;
}

boost::transform_iterator<btcb::iterate_vote_blocks_as_hash, btcb::vote_blocks_vec_iter> btcb::vote::begin () const
{
	return boost::transform_iterator<btcb::iterate_vote_blocks_as_hash, btcb::vote_blocks_vec_iter> (blocks.begin (), btcb::iterate_vote_blocks_as_hash ());
}

boost::transform_iterator<btcb::iterate_vote_blocks_as_hash, btcb::vote_blocks_vec_iter> btcb::vote::end () const
{
	return boost::transform_iterator<btcb::iterate_vote_blocks_as_hash, btcb::vote_blocks_vec_iter> (blocks.end (), btcb::iterate_vote_blocks_as_hash ());
}

btcb::vote_uniquer::vote_uniquer (btcb::block_uniquer & uniquer_a) :
uniquer (uniquer_a)
{
}

std::shared_ptr<btcb::vote> btcb::vote_uniquer::unique (std::shared_ptr<btcb::vote> vote_a)
{
	auto result (vote_a);
	if (result != nullptr)
	{
		if (!result->blocks[0].which ())
		{
			result->blocks[0] = uniquer.unique (boost::get<std::shared_ptr<btcb::block>> (result->blocks[0]));
		}
		btcb::uint256_union key (vote_a->full_hash ());
		std::lock_guard<std::mutex> lock (mutex);
		auto & existing (votes[key]);
		if (auto block_l = existing.lock ())
		{
			result = block_l;
		}
		else
		{
			existing = vote_a;
		}
		for (auto i (0); i < cleanup_count && votes.size () > 0; ++i)
		{
			auto random_offset (btcb::random_pool.GenerateWord32 (0, votes.size () - 1));
			auto existing (std::next (votes.begin (), random_offset));
			if (existing == votes.end ())
			{
				existing = votes.begin ();
			}
			if (existing != votes.end ())
			{
				if (auto block_l = existing->second.lock ())
				{
					// Still live
				}
				else
				{
					votes.erase (existing);
				}
			}
		}
	}
	return result;
}

size_t btcb::vote_uniquer::size ()
{
	std::lock_guard<std::mutex> lock (mutex);
	return votes.size ();
}

btcb::genesis::genesis ()
{
	boost::property_tree::ptree tree;
	std::stringstream istream (btcb::genesis_block);
	boost::property_tree::read_json (istream, tree);
	open = btcb::deserialize_block_json (tree);
	assert (open != nullptr);
}

btcb::block_hash btcb::genesis::hash () const
{
	return open->hash ();
}
