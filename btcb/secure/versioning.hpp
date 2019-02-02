#pragma once

#include <btcb/lib/blocks.hpp>
#include <btcb/node/lmdb.hpp>
#include <btcb/secure/utility.hpp>

namespace btcb
{
class account_info_v1
{
public:
	account_info_v1 ();
	account_info_v1 (MDB_val const &);
	account_info_v1 (btcb::account_info_v1 const &) = default;
	account_info_v1 (btcb::block_hash const &, btcb::block_hash const &, btcb::amount const &, uint64_t);
	void serialize (btcb::stream &) const;
	bool deserialize (btcb::stream &);
	btcb::mdb_val val () const;
	btcb::block_hash head;
	btcb::block_hash rep_block;
	btcb::amount balance;
	uint64_t modified;
};
class pending_info_v3
{
public:
	pending_info_v3 ();
	pending_info_v3 (MDB_val const &);
	pending_info_v3 (btcb::account const &, btcb::amount const &, btcb::account const &);
	void serialize (btcb::stream &) const;
	bool deserialize (btcb::stream &);
	bool operator== (btcb::pending_info_v3 const &) const;
	btcb::mdb_val val () const;
	btcb::account source;
	btcb::amount amount;
	btcb::account destination;
};
// Latest information about an account
class account_info_v5
{
public:
	account_info_v5 ();
	account_info_v5 (MDB_val const &);
	account_info_v5 (btcb::account_info_v5 const &) = default;
	account_info_v5 (btcb::block_hash const &, btcb::block_hash const &, btcb::block_hash const &, btcb::amount const &, uint64_t);
	void serialize (btcb::stream &) const;
	bool deserialize (btcb::stream &);
	btcb::mdb_val val () const;
	btcb::block_hash head;
	btcb::block_hash rep_block;
	btcb::block_hash open_block;
	btcb::amount balance;
	uint64_t modified;
};
}
