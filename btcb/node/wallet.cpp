#include <btcb/lib/utility.hpp>
#include <btcb/node/wallet.hpp>

#include <btcb/node/node.hpp>
#include <btcb/node/wallet.hpp>
#include <btcb/node/xorshift.hpp>

#include <argon2.h>

#include <boost/filesystem.hpp>
#include <boost/polymorphic_cast.hpp>

#include <future>

uint64_t const btcb::work_pool::publish_threshold;

btcb::uint256_union btcb::wallet_store::check (btcb::transaction const & transaction_a)
{
	btcb::wallet_value value (entry_get_raw (transaction_a, btcb::wallet_store::check_special));
	return value.key;
}

btcb::uint256_union btcb::wallet_store::salt (btcb::transaction const & transaction_a)
{
	btcb::wallet_value value (entry_get_raw (transaction_a, btcb::wallet_store::salt_special));
	return value.key;
}

void btcb::wallet_store::wallet_key (btcb::raw_key & prv_a, btcb::transaction const & transaction_a)
{
	std::lock_guard<std::recursive_mutex> lock (mutex);
	btcb::raw_key wallet_l;
	wallet_key_mem.value (wallet_l);
	btcb::raw_key password_l;
	password.value (password_l);
	prv_a.decrypt (wallet_l.data, password_l, salt (transaction_a).owords[0]);
}

void btcb::wallet_store::seed (btcb::raw_key & prv_a, btcb::transaction const & transaction_a)
{
	btcb::wallet_value value (entry_get_raw (transaction_a, btcb::wallet_store::seed_special));
	btcb::raw_key password_l;
	wallet_key (password_l, transaction_a);
	prv_a.decrypt (value.key, password_l, salt (transaction_a).owords[seed_iv_index]);
}

void btcb::wallet_store::seed_set (btcb::transaction const & transaction_a, btcb::raw_key const & prv_a)
{
	btcb::raw_key password_l;
	wallet_key (password_l, transaction_a);
	btcb::uint256_union ciphertext;
	ciphertext.encrypt (prv_a, password_l, salt (transaction_a).owords[seed_iv_index]);
	entry_put_raw (transaction_a, btcb::wallet_store::seed_special, btcb::wallet_value (ciphertext, 0));
	deterministic_clear (transaction_a);
}

btcb::public_key btcb::wallet_store::deterministic_insert (btcb::transaction const & transaction_a)
{
	auto index (deterministic_index_get (transaction_a));
	btcb::raw_key prv;
	deterministic_key (prv, transaction_a, index);
	btcb::public_key result (btcb::pub_key (prv.data));
	while (exists (transaction_a, result))
	{
		++index;
		deterministic_key (prv, transaction_a, index);
		result = btcb::pub_key (prv.data);
	}
	uint64_t marker (1);
	marker <<= 32;
	marker |= index;
	entry_put_raw (transaction_a, result, btcb::wallet_value (btcb::uint256_union (marker), 0));
	++index;
	deterministic_index_set (transaction_a, index);
	return result;
}

void btcb::wallet_store::deterministic_key (btcb::raw_key & prv_a, btcb::transaction const & transaction_a, uint32_t index_a)
{
	assert (valid_password (transaction_a));
	btcb::raw_key seed_l;
	seed (seed_l, transaction_a);
	btcb::deterministic_key (seed_l.data, index_a, prv_a.data);
}

uint32_t btcb::wallet_store::deterministic_index_get (btcb::transaction const & transaction_a)
{
	btcb::wallet_value value (entry_get_raw (transaction_a, btcb::wallet_store::deterministic_index_special));
	return static_cast<uint32_t> (value.key.number () & static_cast<uint32_t> (-1));
}

void btcb::wallet_store::deterministic_index_set (btcb::transaction const & transaction_a, uint32_t index_a)
{
	btcb::uint256_union index_l (index_a);
	btcb::wallet_value value (index_l, 0);
	entry_put_raw (transaction_a, btcb::wallet_store::deterministic_index_special, value);
}

void btcb::wallet_store::deterministic_clear (btcb::transaction const & transaction_a)
{
	btcb::uint256_union key (0);
	for (auto i (begin (transaction_a)), n (end ()); i != n;)
	{
		switch (key_type (btcb::wallet_value (i->second)))
		{
			case btcb::key_type::deterministic:
			{
				btcb::uint256_union key (i->first);
				erase (transaction_a, key);
				i = begin (transaction_a, key);
				break;
			}
			default:
			{
				++i;
				break;
			}
		}
	}
	deterministic_index_set (transaction_a, 0);
}

bool btcb::wallet_store::valid_password (btcb::transaction const & transaction_a)
{
	btcb::raw_key zero;
	zero.data.clear ();
	btcb::raw_key wallet_key_l;
	wallet_key (wallet_key_l, transaction_a);
	btcb::uint256_union check_l;
	check_l.encrypt (zero, wallet_key_l, salt (transaction_a).owords[check_iv_index]);
	bool ok = check (transaction_a) == check_l;
	return ok;
}

bool btcb::wallet_store::attempt_password (btcb::transaction const & transaction_a, std::string const & password_a)
{
	bool result = false;
	{
		std::lock_guard<std::recursive_mutex> lock (mutex);
		btcb::raw_key password_l;
		derive_key (password_l, transaction_a, password_a);
		password.value_set (password_l);
		result = !valid_password (transaction_a);
	}
	if (!result)
	{
		switch (version (transaction_a))
		{
			case version_1:
				upgrade_v1_v2 (transaction_a);
			case version_2:
				upgrade_v2_v3 (transaction_a);
			case version_3:
				upgrade_v3_v4 (transaction_a);
			case version_4:
				break;
			default:
				assert (false);
		}
	}
	return result;
}

bool btcb::wallet_store::rekey (btcb::transaction const & transaction_a, std::string const & password_a)
{
	std::lock_guard<std::recursive_mutex> lock (mutex);
	bool result (false);
	if (valid_password (transaction_a))
	{
		btcb::raw_key password_new;
		derive_key (password_new, transaction_a, password_a);
		btcb::raw_key wallet_key_l;
		wallet_key (wallet_key_l, transaction_a);
		btcb::raw_key password_l;
		password.value (password_l);
		password.value_set (password_new);
		btcb::uint256_union encrypted;
		encrypted.encrypt (wallet_key_l, password_new, salt (transaction_a).owords[0]);
		btcb::raw_key wallet_enc;
		wallet_enc.data = encrypted;
		wallet_key_mem.value_set (wallet_enc);
		entry_put_raw (transaction_a, btcb::wallet_store::wallet_key_special, btcb::wallet_value (encrypted, 0));
	}
	else
	{
		result = true;
	}
	return result;
}

void btcb::wallet_store::derive_key (btcb::raw_key & prv_a, btcb::transaction const & transaction_a, std::string const & password_a)
{
	auto salt_l (salt (transaction_a));
	kdf.phs (prv_a, password_a, salt_l);
}

btcb::fan::fan (btcb::uint256_union const & key, size_t count_a)
{
	std::unique_ptr<btcb::uint256_union> first (new btcb::uint256_union (key));
	for (auto i (1); i < count_a; ++i)
	{
		std::unique_ptr<btcb::uint256_union> entry (new btcb::uint256_union);
		random_pool.GenerateBlock (entry->bytes.data (), entry->bytes.size ());
		*first ^= *entry;
		values.push_back (std::move (entry));
	}
	values.push_back (std::move (first));
}

void btcb::fan::value (btcb::raw_key & prv_a)
{
	std::lock_guard<std::mutex> lock (mutex);
	value_get (prv_a);
}

void btcb::fan::value_get (btcb::raw_key & prv_a)
{
	assert (!mutex.try_lock ());
	prv_a.data.clear ();
	for (auto & i : values)
	{
		prv_a.data ^= *i;
	}
}

void btcb::fan::value_set (btcb::raw_key const & value_a)
{
	std::lock_guard<std::mutex> lock (mutex);
	btcb::raw_key value_l;
	value_get (value_l);
	*(values[0]) ^= value_l.data;
	*(values[0]) ^= value_a.data;
}

// Wallet version number
btcb::uint256_union const btcb::wallet_store::version_special (0);
// Random number used to salt private key encryption
btcb::uint256_union const btcb::wallet_store::salt_special (1);
// Key used to encrypt wallet keys, encrypted itself by the user password
btcb::uint256_union const btcb::wallet_store::wallet_key_special (2);
// Check value used to see if password is valid
btcb::uint256_union const btcb::wallet_store::check_special (3);
// Representative account to be used if we open a new account
btcb::uint256_union const btcb::wallet_store::representative_special (4);
// Wallet seed for deterministic key generation
btcb::uint256_union const btcb::wallet_store::seed_special (5);
// Current key index for deterministic keys
btcb::uint256_union const btcb::wallet_store::deterministic_index_special (6);
int const btcb::wallet_store::special_count (7);
size_t const btcb::wallet_store::check_iv_index (0);
size_t const btcb::wallet_store::seed_iv_index (1);

btcb::wallet_store::wallet_store (bool & init_a, btcb::kdf & kdf_a, btcb::transaction & transaction_a, btcb::account representative_a, unsigned fanout_a, std::string const & wallet_a, std::string const & json_a) :
password (0, fanout_a),
wallet_key_mem (0, fanout_a),
kdf (kdf_a)
{
	init_a = false;
	initialize (transaction_a, init_a, wallet_a);
	if (!init_a)
	{
		MDB_val junk;
		assert (mdb_get (tx (transaction_a), handle, btcb::mdb_val (version_special), &junk) == MDB_NOTFOUND);
		boost::property_tree::ptree wallet_l;
		std::stringstream istream (json_a);
		try
		{
			boost::property_tree::read_json (istream, wallet_l);
		}
		catch (...)
		{
			init_a = true;
		}
		for (auto i (wallet_l.begin ()), n (wallet_l.end ()); i != n; ++i)
		{
			btcb::uint256_union key;
			init_a = key.decode_hex (i->first);
			if (!init_a)
			{
				btcb::uint256_union value;
				init_a = value.decode_hex (wallet_l.get<std::string> (i->first));
				if (!init_a)
				{
					entry_put_raw (transaction_a, key, btcb::wallet_value (value, 0));
				}
				else
				{
					init_a = true;
				}
			}
			else
			{
				init_a = true;
			}
		}
		init_a |= mdb_get (tx (transaction_a), handle, btcb::mdb_val (version_special), &junk) != 0;
		init_a |= mdb_get (tx (transaction_a), handle, btcb::mdb_val (wallet_key_special), &junk) != 0;
		init_a |= mdb_get (tx (transaction_a), handle, btcb::mdb_val (salt_special), &junk) != 0;
		init_a |= mdb_get (tx (transaction_a), handle, btcb::mdb_val (check_special), &junk) != 0;
		init_a |= mdb_get (tx (transaction_a), handle, btcb::mdb_val (representative_special), &junk) != 0;
		btcb::raw_key key;
		key.data.clear ();
		password.value_set (key);
		key.data = entry_get_raw (transaction_a, btcb::wallet_store::wallet_key_special).key;
		wallet_key_mem.value_set (key);
	}
}

btcb::wallet_store::wallet_store (bool & init_a, btcb::kdf & kdf_a, btcb::transaction & transaction_a, btcb::account representative_a, unsigned fanout_a, std::string const & wallet_a) :
password (0, fanout_a),
wallet_key_mem (0, fanout_a),
kdf (kdf_a)
{
	init_a = false;
	initialize (transaction_a, init_a, wallet_a);
	if (!init_a)
	{
		int version_status;
		MDB_val version_value;
		version_status = mdb_get (tx (transaction_a), handle, btcb::mdb_val (version_special), &version_value);
		if (version_status == MDB_NOTFOUND)
		{
			version_put (transaction_a, version_current);
			btcb::uint256_union salt_l;
			random_pool.GenerateBlock (salt_l.bytes.data (), salt_l.bytes.size ());
			entry_put_raw (transaction_a, btcb::wallet_store::salt_special, btcb::wallet_value (salt_l, 0));
			// Wallet key is a fixed random key that encrypts all entries
			btcb::raw_key wallet_key;
			random_pool.GenerateBlock (wallet_key.data.bytes.data (), sizeof (wallet_key.data.bytes));
			btcb::raw_key password_l;
			password_l.data.clear ();
			password.value_set (password_l);
			btcb::raw_key zero;
			zero.data.clear ();
			// Wallet key is encrypted by the user's password
			btcb::uint256_union encrypted;
			encrypted.encrypt (wallet_key, zero, salt_l.owords[0]);
			entry_put_raw (transaction_a, btcb::wallet_store::wallet_key_special, btcb::wallet_value (encrypted, 0));
			btcb::raw_key wallet_key_enc;
			wallet_key_enc.data = encrypted;
			wallet_key_mem.value_set (wallet_key_enc);
			btcb::uint256_union check;
			check.encrypt (zero, wallet_key, salt_l.owords[check_iv_index]);
			entry_put_raw (transaction_a, btcb::wallet_store::check_special, btcb::wallet_value (check, 0));
			entry_put_raw (transaction_a, btcb::wallet_store::representative_special, btcb::wallet_value (representative_a, 0));
			btcb::raw_key seed;
			random_pool.GenerateBlock (seed.data.bytes.data (), seed.data.bytes.size ());
			seed_set (transaction_a, seed);
			entry_put_raw (transaction_a, btcb::wallet_store::deterministic_index_special, btcb::wallet_value (btcb::uint256_union (0), 0));
		}
	}
	btcb::raw_key key;
	key.data = entry_get_raw (transaction_a, btcb::wallet_store::wallet_key_special).key;
	wallet_key_mem.value_set (key);
}

std::vector<btcb::account> btcb::wallet_store::accounts (btcb::transaction const & transaction_a)
{
	std::vector<btcb::account> result;
	for (auto i (begin (transaction_a)), n (end ()); i != n; ++i)
	{
		btcb::account account (i->first);
		result.push_back (account);
	}
	return result;
}

void btcb::wallet_store::initialize (btcb::transaction const & transaction_a, bool & init_a, std::string const & path_a)
{
	assert (strlen (path_a.c_str ()) == path_a.size ());
	auto error (0);
	error |= mdb_dbi_open (tx (transaction_a), path_a.c_str (), MDB_CREATE, &handle);
	init_a = error != 0;
}

bool btcb::wallet_store::is_representative (btcb::transaction const & transaction_a)
{
	return exists (transaction_a, representative (transaction_a));
}

void btcb::wallet_store::representative_set (btcb::transaction const & transaction_a, btcb::account const & representative_a)
{
	entry_put_raw (transaction_a, btcb::wallet_store::representative_special, btcb::wallet_value (representative_a, 0));
}

btcb::account btcb::wallet_store::representative (btcb::transaction const & transaction_a)
{
	btcb::wallet_value value (entry_get_raw (transaction_a, btcb::wallet_store::representative_special));
	return value.key;
}

btcb::public_key btcb::wallet_store::insert_adhoc (btcb::transaction const & transaction_a, btcb::raw_key const & prv)
{
	assert (valid_password (transaction_a));
	btcb::public_key pub (btcb::pub_key (prv.data));
	btcb::raw_key password_l;
	wallet_key (password_l, transaction_a);
	btcb::uint256_union ciphertext;
	ciphertext.encrypt (prv, password_l, pub.owords[0].number ());
	entry_put_raw (transaction_a, pub, btcb::wallet_value (ciphertext, 0));
	return pub;
}

void btcb::wallet_store::insert_watch (btcb::transaction const & transaction_a, btcb::public_key const & pub)
{
	entry_put_raw (transaction_a, pub, btcb::wallet_value (btcb::uint256_union (0), 0));
}

void btcb::wallet_store::erase (btcb::transaction const & transaction_a, btcb::public_key const & pub)
{
	auto status (mdb_del (tx (transaction_a), handle, btcb::mdb_val (pub), nullptr));
	assert (status == 0);
}

btcb::wallet_value btcb::wallet_store::entry_get_raw (btcb::transaction const & transaction_a, btcb::public_key const & pub_a)
{
	btcb::wallet_value result;
	btcb::mdb_val value;
	auto status (mdb_get (tx (transaction_a), handle, btcb::mdb_val (pub_a), value));
	if (status == 0)
	{
		result = btcb::wallet_value (value);
	}
	else
	{
		result.key.clear ();
		result.work = 0;
	}
	return result;
}

void btcb::wallet_store::entry_put_raw (btcb::transaction const & transaction_a, btcb::public_key const & pub_a, btcb::wallet_value const & entry_a)
{
	auto status (mdb_put (tx (transaction_a), handle, btcb::mdb_val (pub_a), entry_a.val (), 0));
	assert (status == 0);
}

btcb::key_type btcb::wallet_store::key_type (btcb::wallet_value const & value_a)
{
	auto number (value_a.key.number ());
	btcb::key_type result;
	auto text (number.convert_to<std::string> ());
	if (number > std::numeric_limits<uint64_t>::max ())
	{
		result = btcb::key_type::adhoc;
	}
	else
	{
		if ((number >> 32).convert_to<uint32_t> () == 1)
		{
			result = btcb::key_type::deterministic;
		}
		else
		{
			result = btcb::key_type::unknown;
		}
	}
	return result;
}

bool btcb::wallet_store::fetch (btcb::transaction const & transaction_a, btcb::public_key const & pub, btcb::raw_key & prv)
{
	auto result (false);
	if (valid_password (transaction_a))
	{
		btcb::wallet_value value (entry_get_raw (transaction_a, pub));
		if (!value.key.is_zero ())
		{
			switch (key_type (value))
			{
				case btcb::key_type::deterministic:
				{
					btcb::raw_key seed_l;
					seed (seed_l, transaction_a);
					uint32_t index (static_cast<uint32_t> (value.key.number () & static_cast<uint32_t> (-1)));
					deterministic_key (prv, transaction_a, index);
					break;
				}
				case btcb::key_type::adhoc:
				{
					// Ad-hoc keys
					btcb::raw_key password_l;
					wallet_key (password_l, transaction_a);
					prv.decrypt (value.key, password_l, pub.owords[0].number ());
					break;
				}
				default:
				{
					result = true;
					break;
				}
			}
		}
		else
		{
			result = true;
		}
	}
	else
	{
		result = true;
	}
	if (!result)
	{
		btcb::public_key compare (btcb::pub_key (prv.data));
		if (!(pub == compare))
		{
			result = true;
		}
	}
	return result;
}

bool btcb::wallet_store::exists (btcb::transaction const & transaction_a, btcb::public_key const & pub)
{
	return !pub.is_zero () && find (transaction_a, pub) != end ();
}

void btcb::wallet_store::serialize_json (btcb::transaction const & transaction_a, std::string & string_a)
{
	boost::property_tree::ptree tree;
	for (btcb::store_iterator<btcb::uint256_union, btcb::wallet_value> i (std::make_unique<btcb::mdb_iterator<btcb::uint256_union, btcb::wallet_value>> (transaction_a, handle)), n (nullptr); i != n; ++i)
	{
		tree.put (i->first.to_string (), i->second.key.to_string ());
	}
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree);
	string_a = ostream.str ();
}

void btcb::wallet_store::write_backup (btcb::transaction const & transaction_a, boost::filesystem::path const & path_a)
{
	std::ofstream backup_file;
	backup_file.open (path_a.string ());
	if (!backup_file.fail ())
	{
		// Set permissions to 600
		boost::system::error_code ec;
		btcb::set_secure_perm_file (path_a, ec);

		std::string json;
		serialize_json (transaction_a, json);
		backup_file << json;
	}
}

bool btcb::wallet_store::move (btcb::transaction const & transaction_a, btcb::wallet_store & other_a, std::vector<btcb::public_key> const & keys)
{
	assert (valid_password (transaction_a));
	assert (other_a.valid_password (transaction_a));
	auto result (false);
	for (auto i (keys.begin ()), n (keys.end ()); i != n; ++i)
	{
		btcb::raw_key prv;
		auto error (other_a.fetch (transaction_a, *i, prv));
		result = result | error;
		if (!result)
		{
			insert_adhoc (transaction_a, prv);
			other_a.erase (transaction_a, *i);
		}
	}
	return result;
}

bool btcb::wallet_store::import (btcb::transaction const & transaction_a, btcb::wallet_store & other_a)
{
	assert (valid_password (transaction_a));
	assert (other_a.valid_password (transaction_a));
	auto result (false);
	for (auto i (other_a.begin (transaction_a)), n (end ()); i != n; ++i)
	{
		btcb::raw_key prv;
		auto error (other_a.fetch (transaction_a, btcb::uint256_union (i->first), prv));
		result = result | error;
		if (!result)
		{
			if (!prv.data.is_zero ())
			{
				insert_adhoc (transaction_a, prv);
			}
			else
			{
				insert_watch (transaction_a, btcb::uint256_union (i->first));
			}
			other_a.erase (transaction_a, btcb::uint256_union (i->first));
		}
	}
	return result;
}

bool btcb::wallet_store::work_get (btcb::transaction const & transaction_a, btcb::public_key const & pub_a, uint64_t & work_a)
{
	auto result (false);
	auto entry (entry_get_raw (transaction_a, pub_a));
	if (!entry.key.is_zero ())
	{
		work_a = entry.work;
	}
	else
	{
		result = true;
	}
	return result;
}

void btcb::wallet_store::work_put (btcb::transaction const & transaction_a, btcb::public_key const & pub_a, uint64_t work_a)
{
	auto entry (entry_get_raw (transaction_a, pub_a));
	assert (!entry.key.is_zero ());
	entry.work = work_a;
	entry_put_raw (transaction_a, pub_a, entry);
}

unsigned btcb::wallet_store::version (btcb::transaction const & transaction_a)
{
	btcb::wallet_value value (entry_get_raw (transaction_a, btcb::wallet_store::version_special));
	auto entry (value.key);
	auto result (static_cast<unsigned> (entry.bytes[31]));
	return result;
}

void btcb::wallet_store::version_put (btcb::transaction const & transaction_a, unsigned version_a)
{
	btcb::uint256_union entry (version_a);
	entry_put_raw (transaction_a, btcb::wallet_store::version_special, btcb::wallet_value (entry, 0));
}

void btcb::wallet_store::upgrade_v1_v2 (btcb::transaction const & transaction_a)
{
	assert (version (transaction_a) == 1);
	btcb::raw_key zero_password;
	btcb::wallet_value value (entry_get_raw (transaction_a, btcb::wallet_store::wallet_key_special));
	btcb::raw_key kdf;
	kdf.data.clear ();
	zero_password.decrypt (value.key, kdf, salt (transaction_a).owords[0]);
	derive_key (kdf, transaction_a, "");
	btcb::raw_key empty_password;
	empty_password.decrypt (value.key, kdf, salt (transaction_a).owords[0]);
	for (auto i (begin (transaction_a)), n (end ()); i != n; ++i)
	{
		btcb::public_key key (i->first);
		btcb::raw_key prv;
		if (fetch (transaction_a, key, prv))
		{
			// Key failed to decrypt despite valid password
			btcb::wallet_value data (entry_get_raw (transaction_a, key));
			prv.decrypt (data.key, zero_password, salt (transaction_a).owords[0]);
			btcb::public_key compare (btcb::pub_key (prv.data));
			if (compare == key)
			{
				// If we successfully decrypted it, rewrite the key back with the correct wallet key
				insert_adhoc (transaction_a, prv);
			}
			else
			{
				// Also try the empty password
				btcb::wallet_value data (entry_get_raw (transaction_a, key));
				prv.decrypt (data.key, empty_password, salt (transaction_a).owords[0]);
				btcb::public_key compare (btcb::pub_key (prv.data));
				if (compare == key)
				{
					// If we successfully decrypted it, rewrite the key back with the correct wallet key
					insert_adhoc (transaction_a, prv);
				}
			}
		}
	}
	version_put (transaction_a, 2);
}

void btcb::wallet_store::upgrade_v2_v3 (btcb::transaction const & transaction_a)
{
	assert (version (transaction_a) == 2);
	btcb::raw_key seed;
	random_pool.GenerateBlock (seed.data.bytes.data (), seed.data.bytes.size ());
	seed_set (transaction_a, seed);
	entry_put_raw (transaction_a, btcb::wallet_store::deterministic_index_special, btcb::wallet_value (btcb::uint256_union (0), 0));
	version_put (transaction_a, 3);
}

void btcb::wallet_store::upgrade_v3_v4 (btcb::transaction const & transaction_a)
{
	assert (version (transaction_a) == 3);
	version_put (transaction_a, 4);
	assert (valid_password (transaction_a));
	btcb::raw_key seed;
	btcb::wallet_value value (entry_get_raw (transaction_a, btcb::wallet_store::seed_special));
	btcb::raw_key password_l;
	wallet_key (password_l, transaction_a);
	seed.decrypt (value.key, password_l, salt (transaction_a).owords[0]);
	btcb::uint256_union ciphertext;
	ciphertext.encrypt (seed, password_l, salt (transaction_a).owords[seed_iv_index]);
	entry_put_raw (transaction_a, btcb::wallet_store::seed_special, btcb::wallet_value (ciphertext, 0));
	for (auto i (begin (transaction_a)), n (end ()); i != n; ++i)
	{
		btcb::wallet_value value (i->second);
		if (!value.key.is_zero ())
		{
			switch (key_type (i->second))
			{
				case btcb::key_type::adhoc:
				{
					btcb::raw_key key;
					if (fetch (transaction_a, btcb::public_key (i->first), key))
					{
						// Key failed to decrypt despite valid password
						key.decrypt (value.key, password_l, salt (transaction_a).owords[0]);
						btcb::uint256_union new_key_ciphertext;
						new_key_ciphertext.encrypt (key, password_l, (btcb::uint256_union (i->first)).owords[0].number ());
						btcb::wallet_value new_value (new_key_ciphertext, value.work);
						erase (transaction_a, btcb::public_key (i->first));
						entry_put_raw (transaction_a, btcb::public_key (i->first), new_value);
					}
				}
				case btcb::key_type::deterministic:
					break;
				default:
					assert (false);
			}
		}
	}
}

void btcb::kdf::phs (btcb::raw_key & result_a, std::string const & password_a, btcb::uint256_union const & salt_a)
{
	std::lock_guard<std::mutex> lock (mutex);
	auto success (argon2_hash (1, btcb::wallet_store::kdf_work, 1, password_a.data (), password_a.size (), salt_a.bytes.data (), salt_a.bytes.size (), result_a.data.bytes.data (), result_a.data.bytes.size (), NULL, 0, Argon2_d, 0x10));
	assert (success == 0);
	(void)success;
}

btcb::wallet::wallet (bool & init_a, btcb::transaction & transaction_a, btcb::wallets & wallets_a, std::string const & wallet_a) :
lock_observer ([](bool, bool) {}),
store (init_a, wallets_a.kdf, transaction_a, wallets_a.node.config.random_representative (), wallets_a.node.config.password_fanout, wallet_a),
wallets (wallets_a)
{
}

btcb::wallet::wallet (bool & init_a, btcb::transaction & transaction_a, btcb::wallets & wallets_a, std::string const & wallet_a, std::string const & json) :
lock_observer ([](bool, bool) {}),
store (init_a, wallets_a.kdf, transaction_a, wallets_a.node.config.random_representative (), wallets_a.node.config.password_fanout, wallet_a, json),
wallets (wallets_a)
{
}

void btcb::wallet::enter_initial_password ()
{
	std::lock_guard<std::recursive_mutex> lock (store.mutex);
	btcb::raw_key password_l;
	store.password.value (password_l);
	if (password_l.data.is_zero ())
	{
		auto transaction (wallets.tx_begin_write ());
		if (store.valid_password (transaction))
		{
			// Newly created wallets have a zero key
			store.rekey (transaction, "");
		}
		else
		{
			enter_password (transaction, "");
		}
	}
}

bool btcb::wallet::enter_password (btcb::transaction const & transaction_a, std::string const & password_a)
{
	auto result (store.attempt_password (transaction_a, password_a));
	if (!result)
	{
		auto this_l (shared_from_this ());
		wallets.node.background ([this_l]() {
			this_l->search_pending ();
		});
	}
	lock_observer (result, password_a.empty ());
	return result;
}

btcb::public_key btcb::wallet::deterministic_insert (btcb::transaction const & transaction_a, bool generate_work_a)
{
	btcb::public_key key (0);
	if (store.valid_password (transaction_a))
	{
		key = store.deterministic_insert (transaction_a);
		if (generate_work_a)
		{
			work_ensure (key, key);
		}
	}
	return key;
}

btcb::public_key btcb::wallet::deterministic_insert (bool generate_work_a)
{
	auto transaction (wallets.tx_begin_write ());
	auto result (deterministic_insert (transaction, generate_work_a));
	return result;
}

btcb::public_key btcb::wallet::insert_adhoc (btcb::transaction const & transaction_a, btcb::raw_key const & key_a, bool generate_work_a)
{
	btcb::public_key key (0);
	if (store.valid_password (transaction_a))
	{
		key = store.insert_adhoc (transaction_a, key_a);
		if (generate_work_a)
		{
			work_ensure (key, wallets.node.ledger.latest_root (transaction_a, key));
		}
	}
	return key;
}

btcb::public_key btcb::wallet::insert_adhoc (btcb::raw_key const & account_a, bool generate_work_a)
{
	auto transaction (wallets.tx_begin_write ());
	auto result (insert_adhoc (transaction, account_a, generate_work_a));
	return result;
}

void btcb::wallet::insert_watch (btcb::transaction const & transaction_a, btcb::public_key const & pub_a)
{
	store.insert_watch (transaction_a, pub_a);
}

bool btcb::wallet::exists (btcb::public_key const & account_a)
{
	auto transaction (wallets.tx_begin_read ());
	return store.exists (transaction, account_a);
}

bool btcb::wallet::import (std::string const & json_a, std::string const & password_a)
{
	auto error (false);
	std::unique_ptr<btcb::wallet_store> temp;
	{
		auto transaction (wallets.tx_begin_write ());
		btcb::uint256_union id;
		random_pool.GenerateBlock (id.bytes.data (), id.bytes.size ());
		temp.reset (new btcb::wallet_store (error, wallets.node.wallets.kdf, transaction, 0, 1, id.to_string (), json_a));
	}
	if (!error)
	{
		auto transaction (wallets.tx_begin_read ());
		error = temp->attempt_password (transaction, password_a);
	}
	auto transaction (wallets.tx_begin_write ());
	if (!error)
	{
		error = store.import (transaction, *temp);
	}
	temp->destroy (transaction);
	return error;
}

void btcb::wallet::serialize (std::string & json_a)
{
	auto transaction (wallets.tx_begin_read ());
	store.serialize_json (transaction, json_a);
}

void btcb::wallet_store::destroy (btcb::transaction const & transaction_a)
{
	auto status (mdb_drop (tx (transaction_a), handle, 1));
	assert (status == 0);
	handle = 0;
}

std::shared_ptr<btcb::block> btcb::wallet::receive_action (btcb::block const & send_a, btcb::account const & representative_a, btcb::uint128_union const & amount_a, bool generate_work_a)
{
	btcb::account account;
	auto hash (send_a.hash ());
	std::shared_ptr<btcb::block> block;
	if (wallets.node.config.receive_minimum.number () <= amount_a.number ())
	{
		auto transaction (wallets.node.ledger.store.tx_begin_read ());
		btcb::pending_info pending_info;
		if (wallets.node.store.block_exists (transaction, hash))
		{
			account = wallets.node.ledger.block_destination (transaction, send_a);
			if (!wallets.node.ledger.store.pending_get (transaction, btcb::pending_key (account, hash), pending_info))
			{
				btcb::raw_key prv;
				if (!store.fetch (transaction, account, prv))
				{
					uint64_t cached_work (0);
					store.work_get (transaction, account, cached_work);
					btcb::account_info info;
					auto new_account (wallets.node.ledger.store.account_get (transaction, account, info));
					if (!new_account)
					{
						std::shared_ptr<btcb::block> rep_block = wallets.node.ledger.store.block_get (transaction, info.rep_block);
						assert (rep_block != nullptr);
						block.reset (new btcb::state_block (account, info.head, rep_block->representative (), info.balance.number () + pending_info.amount.number (), hash, prv, account, cached_work));
					}
					else
					{
						block.reset (new btcb::state_block (account, 0, representative_a, pending_info.amount, hash, prv, account, cached_work));
					}
				}
				else
				{
					BOOST_LOG (wallets.node.log) << "Unable to receive, wallet locked";
				}
			}
			else
			{
				// Ledger doesn't have this marked as available to receive anymore
			}
		}
		else
		{
			// Ledger doesn't have this block anymore.
		}
	}
	else
	{
		BOOST_LOG (wallets.node.log) << boost::str (boost::format ("Not receiving block %1% due to minimum receive threshold") % hash.to_string ());
		// Someone sent us something below the threshold of receiving
	}
	if (block != nullptr)
	{
		if (btcb::work_validate (*block))
		{
			wallets.node.work_generate_blocking (*block);
		}
		wallets.node.process_active (block);
		wallets.node.block_processor.flush ();
		if (generate_work_a)
		{
			work_ensure (account, block->hash ());
		}
	}
	return block;
}

std::shared_ptr<btcb::block> btcb::wallet::change_action (btcb::account const & source_a, btcb::account const & representative_a, bool generate_work_a)
{
	std::shared_ptr<btcb::block> block;
	{
		auto transaction (wallets.tx_begin_read ());
		if (store.valid_password (transaction))
		{
			auto existing (store.find (transaction, source_a));
			if (existing != store.end () && !wallets.node.ledger.latest (transaction, source_a).is_zero ())
			{
				btcb::account_info info;
				auto error1 (wallets.node.ledger.store.account_get (transaction, source_a, info));
				assert (!error1);
				btcb::raw_key prv;
				auto error2 (store.fetch (transaction, source_a, prv));
				assert (!error2);
				uint64_t cached_work (0);
				store.work_get (transaction, source_a, cached_work);
				block.reset (new btcb::state_block (source_a, info.head, representative_a, info.balance, 0, prv, source_a, cached_work));
			}
		}
	}
	if (block != nullptr)
	{
		if (btcb::work_validate (*block))
		{
			wallets.node.work_generate_blocking (*block);
		}
		wallets.node.process_active (block);
		wallets.node.block_processor.flush ();
		if (generate_work_a)
		{
			work_ensure (source_a, block->hash ());
		}
	}
	return block;
}

std::shared_ptr<btcb::block> btcb::wallet::send_action (btcb::account const & source_a, btcb::account const & account_a, btcb::uint128_t const & amount_a, bool generate_work_a, boost::optional<std::string> id_a)
{
	std::shared_ptr<btcb::block> block;
	boost::optional<btcb::mdb_val> id_mdb_val;
	if (id_a)
	{
		id_mdb_val = btcb::mdb_val (id_a->size (), const_cast<char *> (id_a->data ()));
	}
	bool error = false;
	bool cached_block = false;
	{
		auto transaction (wallets.tx_begin ((bool)id_mdb_val));
		if (id_mdb_val)
		{
			btcb::mdb_val result;
			auto status (mdb_get (wallets.env.tx (transaction), wallets.node.wallets.send_action_ids, *id_mdb_val, result));
			if (status == 0)
			{
				btcb::uint256_union hash (result);
				block = wallets.node.store.block_get (transaction, hash);
				if (block != nullptr)
				{
					cached_block = true;
					wallets.node.network.republish_block (block);
				}
			}
			else if (status != MDB_NOTFOUND)
			{
				error = true;
			}
		}
		if (!error && block == nullptr)
		{
			if (store.valid_password (transaction))
			{
				auto existing (store.find (transaction, source_a));
				if (existing != store.end ())
				{
					auto balance (wallets.node.ledger.account_balance (transaction, source_a));
					if (!balance.is_zero () && balance >= amount_a)
					{
						btcb::account_info info;
						auto error1 (wallets.node.ledger.store.account_get (transaction, source_a, info));
						assert (!error1);
						btcb::raw_key prv;
						auto error2 (store.fetch (transaction, source_a, prv));
						assert (!error2);
						std::shared_ptr<btcb::block> rep_block = wallets.node.ledger.store.block_get (transaction, info.rep_block);
						assert (rep_block != nullptr);
						uint64_t cached_work (0);
						store.work_get (transaction, source_a, cached_work);
						block.reset (new btcb::state_block (source_a, info.head, rep_block->representative (), balance - amount_a, account_a, prv, source_a, cached_work));
						if (id_mdb_val && block != nullptr)
						{
							auto status (mdb_put (wallets.env.tx (transaction), wallets.node.wallets.send_action_ids, *id_mdb_val, btcb::mdb_val (block->hash ()), 0));
							if (status != 0)
							{
								block = nullptr;
								error = true;
							}
						}
					}
				}
			}
		}
	}
	if (!error && block != nullptr && !cached_block)
	{
		if (btcb::work_validate (*block))
		{
			wallets.node.work_generate_blocking (*block);
		}
		wallets.node.process_active (block);
		wallets.node.block_processor.flush ();
		if (generate_work_a)
		{
			work_ensure (source_a, block->hash ());
		}
	}
	return block;
}

bool btcb::wallet::change_sync (btcb::account const & source_a, btcb::account const & representative_a)
{
	std::promise<bool> result;
	change_async (source_a, representative_a, [&result](std::shared_ptr<btcb::block> block_a) {
		result.set_value (block_a == nullptr);
	},
	true);
	return result.get_future ().get ();
}

void btcb::wallet::change_async (btcb::account const & source_a, btcb::account const & representative_a, std::function<void(std::shared_ptr<btcb::block>)> const & action_a, bool generate_work_a)
{
	wallets.node.wallets.queue_wallet_action (btcb::wallets::high_priority, shared_from_this (), [source_a, representative_a, action_a, generate_work_a](btcb::wallet & wallet_a) {
		auto block (wallet_a.change_action (source_a, representative_a, generate_work_a));
		action_a (block);
	});
}

bool btcb::wallet::receive_sync (std::shared_ptr<btcb::block> block_a, btcb::account const & representative_a, btcb::uint128_t const & amount_a)
{
	std::promise<bool> result;
	receive_async (block_a, representative_a, amount_a, [&result](std::shared_ptr<btcb::block> block_a) {
		result.set_value (block_a == nullptr);
	},
	true);
	return result.get_future ().get ();
}

void btcb::wallet::receive_async (std::shared_ptr<btcb::block> block_a, btcb::account const & representative_a, btcb::uint128_t const & amount_a, std::function<void(std::shared_ptr<btcb::block>)> const & action_a, bool generate_work_a)
{
	//assert (dynamic_cast<btcb::send_block *> (block_a.get ()) != nullptr);
	wallets.node.wallets.queue_wallet_action (amount_a, shared_from_this (), [block_a, representative_a, amount_a, action_a, generate_work_a](btcb::wallet & wallet_a) {
		auto block (wallet_a.receive_action (*static_cast<btcb::block *> (block_a.get ()), representative_a, amount_a, generate_work_a));
		action_a (block);
	});
}

btcb::block_hash btcb::wallet::send_sync (btcb::account const & source_a, btcb::account const & account_a, btcb::uint128_t const & amount_a)
{
	std::promise<btcb::block_hash> result;
	send_async (source_a, account_a, amount_a, [&result](std::shared_ptr<btcb::block> block_a) {
		result.set_value (block_a->hash ());
	},
	true);
	return result.get_future ().get ();
}

void btcb::wallet::send_async (btcb::account const & source_a, btcb::account const & account_a, btcb::uint128_t const & amount_a, std::function<void(std::shared_ptr<btcb::block>)> const & action_a, bool generate_work_a, boost::optional<std::string> id_a)
{
	wallets.node.wallets.queue_wallet_action (btcb::wallets::high_priority, shared_from_this (), [source_a, account_a, amount_a, action_a, generate_work_a, id_a](btcb::wallet & wallet_a) {
		auto block (wallet_a.send_action (source_a, account_a, amount_a, generate_work_a, id_a));
		action_a (block);
	});
}

// Update work for account if latest root is root_a
void btcb::wallet::work_update (btcb::transaction const & transaction_a, btcb::account const & account_a, btcb::block_hash const & root_a, uint64_t work_a)
{
	assert (!btcb::work_validate (root_a, work_a));
	assert (store.exists (transaction_a, account_a));
	auto latest (wallets.node.ledger.latest_root (transaction_a, account_a));
	if (latest == root_a)
	{
		store.work_put (transaction_a, account_a, work_a);
	}
	else
	{
		BOOST_LOG (wallets.node.log) << "Cached work no longer valid, discarding";
	}
}

void btcb::wallet::work_ensure (btcb::account const & account_a, btcb::block_hash const & hash_a)
{
	wallets.node.wallets.queue_wallet_action (btcb::wallets::generate_priority, shared_from_this (), [account_a, hash_a](btcb::wallet & wallet_a) {
		wallet_a.work_cache_blocking (account_a, hash_a);
	});
}

bool btcb::wallet::search_pending ()
{
	auto transaction (wallets.tx_begin_read ());
	auto result (!store.valid_password (transaction));
	if (!result)
	{
		BOOST_LOG (wallets.node.log) << "Beginning pending block search";
		for (auto i (store.begin (transaction)), n (store.end ()); i != n; ++i)
		{
			auto transaction (wallets.node.store.tx_begin_read ());
			btcb::account account (i->first);
			// Don't search pending for watch-only accounts
			if (!btcb::wallet_value (i->second).key.is_zero ())
			{
				for (auto j (wallets.node.store.pending_begin (transaction, btcb::pending_key (account, 0))); btcb::pending_key (j->first).account == account; ++j)
				{
					btcb::pending_key key (j->first);
					auto hash (key.hash);
					btcb::pending_info pending (j->second);
					auto amount (pending.amount.number ());
					if (wallets.node.config.receive_minimum.number () <= amount)
					{
						BOOST_LOG (wallets.node.log) << boost::str (boost::format ("Found a pending block %1% for account %2%") % hash.to_string () % pending.source.to_account ());
						wallets.node.block_confirm (wallets.node.store.block_get (transaction, hash));
					}
				}
			}
		}
		BOOST_LOG (wallets.node.log) << "Pending block search phase complete";
	}
	else
	{
		BOOST_LOG (wallets.node.log) << "Stopping search, wallet is locked";
	}
	return result;
}

void btcb::wallet::init_free_accounts (btcb::transaction const & transaction_a)
{
	free_accounts.clear ();
	for (auto i (store.begin (transaction_a)), n (store.end ()); i != n; ++i)
	{
		free_accounts.insert (btcb::uint256_union (i->first));
	}
}

btcb::public_key btcb::wallet::change_seed (btcb::transaction const & transaction_a, btcb::raw_key const & prv_a)
{
	store.seed_set (transaction_a, prv_a);
	auto account = deterministic_insert (transaction_a);
	uint32_t count (0);
	for (uint32_t i (1), n (64); i < n; ++i)
	{
		btcb::raw_key prv;
		store.deterministic_key (prv, transaction_a, i);
		btcb::keypair pair (prv.data.to_string ());
		// Check if account received at least 1 block
		auto latest (wallets.node.ledger.latest (transaction_a, pair.pub));
		if (!latest.is_zero ())
		{
			count = i;
			// i + 64 - Check additional 64 accounts
			// i/64 - Check additional accounts for large wallets. I.e. 64000/64 = 1000 accounts to check
			n = i + 64 + (i / 64);
		}
		else
		{
			// Check if there are pending blocks for account
			for (auto ii (wallets.node.store.pending_begin (transaction_a, btcb::pending_key (pair.pub, 0))); btcb::pending_key (ii->first).account == pair.pub; ++ii)
			{
				count = i;
				n = i + 64 + (i / 64);
				break;
			}
		}
	}
	for (uint32_t i (0); i < count; ++i)
	{
		// Generate work for first 4 accounts only to prevent weak CPU nodes stuck
		account = deterministic_insert (transaction_a, i < 4);
	}

	return account;
}

bool btcb::wallet::live ()
{
	return store.handle != 0;
}

void btcb::wallet::work_cache_blocking (btcb::account const & account_a, btcb::block_hash const & root_a)
{
	auto begin (std::chrono::steady_clock::now ());
	auto work (wallets.node.work_generate_blocking (root_a));
	if (wallets.node.config.logging.work_generation_time ())
	{
		/*
		 * The difficulty parameter is the second parameter for `work_generate_blocking()`,
		 * currently we don't supply one so we must fetch the default value.
		 */
		auto difficulty (btcb::work_pool::publish_threshold);

		BOOST_LOG (wallets.node.log) << "Work generation for " << root_a.to_string () << ", with a difficulty of " << difficulty << " complete: " << (std::chrono::duration_cast<std::chrono::microseconds> (std::chrono::steady_clock::now () - begin).count ()) << " us";
	}
	auto transaction (wallets.tx_begin_write ());
	if (store.exists (transaction, account_a))
	{
		work_update (transaction, account_a, root_a, work);
	}
}

btcb::wallets::wallets (bool & error_a, btcb::node & node_a) :
observer ([](bool) {}),
node (node_a),
env (boost::polymorphic_downcast<btcb::mdb_store *> (node_a.store_impl.get ())->env),
stopped (false),
thread ([this]() {
	btcb::thread_role::set (btcb::thread_role::name::wallet_actions);
	do_wallet_actions ();
})
{
	if (!error_a)
	{
		auto transaction (node.store.tx_begin_write ());
		auto status (mdb_dbi_open (env.tx (transaction), nullptr, MDB_CREATE, &handle));
		status |= mdb_dbi_open (env.tx (transaction), "send_action_ids", MDB_CREATE, &send_action_ids);
		assert (status == 0);
		std::string beginning (btcb::uint256_union (0).to_string ());
		std::string end ((btcb::uint256_union (btcb::uint256_t (0) - btcb::uint256_t (1))).to_string ());
		btcb::store_iterator<std::array<char, 64>, btcb::mdb_val::no_value> i (std::make_unique<btcb::mdb_iterator<std::array<char, 64>, btcb::mdb_val::no_value>> (transaction, handle, btcb::mdb_val (beginning.size (), const_cast<char *> (beginning.c_str ()))));
		btcb::store_iterator<std::array<char, 64>, btcb::mdb_val::no_value> n (std::make_unique<btcb::mdb_iterator<std::array<char, 64>, btcb::mdb_val::no_value>> (transaction, handle, btcb::mdb_val (end.size (), const_cast<char *> (end.c_str ()))));
		for (; i != n; ++i)
		{
			btcb::uint256_union id;
			std::string text (i->first.data (), i->first.size ());
			auto error (id.decode_hex (text));
			assert (!error);
			assert (items.find (id) == items.end ());
			auto wallet (std::make_shared<btcb::wallet> (error, transaction, *this, text));
			if (!error)
			{
				items[id] = wallet;
			}
			else
			{
				// Couldn't open wallet
			}
		}
	}
	for (auto i (items.begin ()), n (items.end ()); i != n; ++i)
	{
		i->second->enter_initial_password ();
	}
}

btcb::wallets::~wallets ()
{
	stop ();
}

std::shared_ptr<btcb::wallet> btcb::wallets::open (btcb::uint256_union const & id_a)
{
	std::shared_ptr<btcb::wallet> result;
	auto existing (items.find (id_a));
	if (existing != items.end ())
	{
		result = existing->second;
	}
	return result;
}

std::shared_ptr<btcb::wallet> btcb::wallets::create (btcb::uint256_union const & id_a)
{
	assert (items.find (id_a) == items.end ());
	std::shared_ptr<btcb::wallet> result;
	bool error;
	{
		auto transaction (node.store.tx_begin_write ());
		result = std::make_shared<btcb::wallet> (error, transaction, *this, id_a.to_string ());
	}
	if (!error)
	{
		items[id_a] = result;
		result->enter_initial_password ();
	}
	return result;
}

bool btcb::wallets::search_pending (btcb::uint256_union const & wallet_a)
{
	auto result (false);
	auto existing (items.find (wallet_a));
	result = existing == items.end ();
	if (!result)
	{
		auto wallet (existing->second);
		result = wallet->search_pending ();
	}
	return result;
}

void btcb::wallets::search_pending_all ()
{
	for (auto i : items)
	{
		i.second->search_pending ();
	}
}

void btcb::wallets::destroy (btcb::uint256_union const & id_a)
{
	std::lock_guard<std::mutex> lock (mutex);
	auto transaction (node.store.tx_begin_write ());
	auto existing (items.find (id_a));
	assert (existing != items.end ());
	auto wallet (existing->second);
	items.erase (existing);
	wallet->store.destroy (transaction);
}

void btcb::wallets::do_wallet_actions ()
{
	std::unique_lock<std::mutex> lock (mutex);
	while (!stopped)
	{
		if (!actions.empty ())
		{
			auto first (actions.begin ());
			auto wallet (first->second.first);
			auto current (std::move (first->second.second));
			actions.erase (first);
			if (wallet->live ())
			{
				lock.unlock ();
				observer (true);
				current (*wallet);
				observer (false);
				lock.lock ();
			}
		}
		else
		{
			condition.wait (lock);
		}
	}
}

void btcb::wallets::queue_wallet_action (btcb::uint128_t const & amount_a, std::shared_ptr<btcb::wallet> wallet_a, std::function<void(btcb::wallet &)> const & action_a)
{
	{
		std::lock_guard<std::mutex> lock (mutex);
		actions.insert (std::make_pair (amount_a, std::make_pair (wallet_a, std::move (action_a))));
	}
	condition.notify_all ();
}

void btcb::wallets::foreach_representative (btcb::transaction const & transaction_a, std::function<void(btcb::public_key const & pub_a, btcb::raw_key const & prv_a)> const & action_a)
{
	for (auto i (items.begin ()), n (items.end ()); i != n; ++i)
	{
		auto & wallet (*i->second);
		for (auto j (wallet.store.begin (transaction_a)), m (wallet.store.end ()); j != m; ++j)
		{
			btcb::account account (j->first);
			if (!node.ledger.weight (transaction_a, account).is_zero ())
			{
				if (wallet.store.valid_password (transaction_a))
				{
					btcb::raw_key prv;
					auto error (wallet.store.fetch (transaction_a, btcb::uint256_union (j->first), prv));
					assert (!error);
					action_a (btcb::uint256_union (j->first), prv);
				}
				else
				{
					static auto last_log = std::chrono::steady_clock::time_point ();
					if (last_log < std::chrono::steady_clock::now () - std::chrono::seconds (60))
					{
						last_log = std::chrono::steady_clock::now ();
						BOOST_LOG (node.log) << boost::str (boost::format ("Representative locked inside wallet %1%") % i->first.to_string ());
					}
				}
			}
		}
	}
}

bool btcb::wallets::exists (btcb::transaction const & transaction_a, btcb::public_key const & account_a)
{
	auto result (false);
	for (auto i (items.begin ()), n (items.end ()); !result && i != n; ++i)
	{
		result = i->second->store.exists (transaction_a, account_a);
	}
	return result;
}

void btcb::wallets::stop ()
{
	{
		std::lock_guard<std::mutex> lock (mutex);
		stopped = true;
		actions.clear ();
	}
	condition.notify_all ();
	if (thread.joinable ())
	{
		thread.join ();
	}
}

btcb::transaction btcb::wallets::tx_begin_write ()
{
	return tx_begin (true);
}

btcb::transaction btcb::wallets::tx_begin_read ()
{
	return tx_begin (false);
}

btcb::transaction btcb::wallets::tx_begin (bool write_a)
{
	return env.tx_begin (write_a);
}

void btcb::wallets::clear_send_ids (btcb::transaction const & transaction_a)
{
	auto status (mdb_drop (env.tx (transaction_a), send_action_ids, 0));
	assert (status == 0);
}

btcb::uint128_t const btcb::wallets::generate_priority = std::numeric_limits<btcb::uint128_t>::max ();
btcb::uint128_t const btcb::wallets::high_priority = std::numeric_limits<btcb::uint128_t>::max () - 1;

btcb::store_iterator<btcb::uint256_union, btcb::wallet_value> btcb::wallet_store::begin (btcb::transaction const & transaction_a)
{
	btcb::store_iterator<btcb::uint256_union, btcb::wallet_value> result (std::make_unique<btcb::mdb_iterator<btcb::uint256_union, btcb::wallet_value>> (transaction_a, handle, btcb::mdb_val (btcb::uint256_union (special_count))));
	return result;
}

btcb::store_iterator<btcb::uint256_union, btcb::wallet_value> btcb::wallet_store::begin (btcb::transaction const & transaction_a, btcb::uint256_union const & key)
{
	btcb::store_iterator<btcb::uint256_union, btcb::wallet_value> result (std::make_unique<btcb::mdb_iterator<btcb::uint256_union, btcb::wallet_value>> (transaction_a, handle, btcb::mdb_val (key)));
	return result;
}

btcb::store_iterator<btcb::uint256_union, btcb::wallet_value> btcb::wallet_store::find (btcb::transaction const & transaction_a, btcb::uint256_union const & key)
{
	auto result (begin (transaction_a, key));
	btcb::store_iterator<btcb::uint256_union, btcb::wallet_value> end (nullptr);
	if (result != end)
	{
		if (btcb::uint256_union (result->first) == key)
		{
			return result;
		}
		else
		{
			return end;
		}
	}
	else
	{
		return end;
	}
	return result;
}

btcb::store_iterator<btcb::uint256_union, btcb::wallet_value> btcb::wallet_store::end ()
{
	return btcb::store_iterator<btcb::uint256_union, btcb::wallet_value> (nullptr);
}

MDB_txn * btcb::wallet_store::tx (btcb::transaction const & transaction_a) const
{
	auto result (boost::polymorphic_downcast<btcb::mdb_txn *> (transaction_a.impl.get ()));
	return *result;
}
