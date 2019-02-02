#pragma once

#include <btcb/secure/common.hpp>
#include <stack>

namespace btcb
{
class transaction;
class block_store;

/**
 * Summation visitor for blocks, supporting amount and balance computations. These
 * computations are mutually dependant. The natural solution is to use mutual recursion
 * between balance and amount visitors, but this leads to very deep stacks. Hence, the
 * summation visitor uses an iterative approach.
 */
class summation_visitor : public btcb::block_visitor
{
	enum summation_type
	{
		invalid = 0,
		balance = 1,
		amount = 2
	};

	/** Represents an invocation frame */
	class frame
	{
	public:
		frame (summation_type type_a, btcb::block_hash balance_hash_a, btcb::block_hash amount_hash_a) :
		type (type_a), balance_hash (balance_hash_a), amount_hash (amount_hash_a)
		{
		}

		/** The summation type guides the block visitor handlers */
		summation_type type{ invalid };
		/** Accumulated balance or amount */
		btcb::uint128_t sum{ 0 };
		/** The current balance hash */
		btcb::block_hash balance_hash{ 0 };
		/** The current amount hash */
		btcb::block_hash amount_hash{ 0 };
		/** If true, this frame is awaiting an invocation result */
		bool awaiting_result{ false };
		/** Set by the invoked frame, representing the return value */
		btcb::uint128_t incoming_result{ 0 };
	};

public:
	summation_visitor (btcb::transaction const &, btcb::block_store &);
	virtual ~summation_visitor () = default;
	/** Computes the balance as of \p block_hash */
	btcb::uint128_t compute_balance (btcb::block_hash const & block_hash);
	/** Computes the amount delta between \p block_hash and its predecessor */
	btcb::uint128_t compute_amount (btcb::block_hash const & block_hash);

protected:
	btcb::transaction const & transaction;
	btcb::block_store & store;

	/** The final result */
	btcb::uint128_t result{ 0 };
	/** The current invocation frame */
	frame * current{ nullptr };
	/** Invocation frames */
	std::stack<frame> frames;
	/** Push a copy of \p hash of the given summation \p type */
	btcb::summation_visitor::frame push (btcb::summation_visitor::summation_type type, btcb::block_hash const & hash);
	void sum_add (btcb::uint128_t addend_a);
	void sum_set (btcb::uint128_t value_a);
	/** The epilogue yields the result to previous frame, if any */
	void epilogue ();

	btcb::uint128_t compute_internal (btcb::summation_visitor::summation_type type, btcb::block_hash const &);
	void send_block (btcb::send_block const &) override;
	void receive_block (btcb::receive_block const &) override;
	void open_block (btcb::open_block const &) override;
	void change_block (btcb::change_block const &) override;
	void state_block (btcb::state_block const &) override;
};

/**
 * Determine the representative for this block
 */
class representative_visitor : public btcb::block_visitor
{
public:
	representative_visitor (btcb::transaction const & transaction_a, btcb::block_store & store_a);
	virtual ~representative_visitor () = default;
	void compute (btcb::block_hash const & hash_a);
	void send_block (btcb::send_block const & block_a) override;
	void receive_block (btcb::receive_block const & block_a) override;
	void open_block (btcb::open_block const & block_a) override;
	void change_block (btcb::change_block const & block_a) override;
	void state_block (btcb::state_block const & block_a) override;
	btcb::transaction const & transaction;
	btcb::block_store & store;
	btcb::block_hash current;
	btcb::block_hash result;
};
template <typename T, typename U>
class store_iterator_impl
{
public:
	virtual ~store_iterator_impl () = default;
	virtual btcb::store_iterator_impl<T, U> & operator++ () = 0;
	virtual bool operator== (btcb::store_iterator_impl<T, U> const & other_a) const = 0;
	virtual bool is_end_sentinal () const = 0;
	virtual void fill (std::pair<T, U> &) const = 0;
	btcb::store_iterator_impl<T, U> & operator= (btcb::store_iterator_impl<T, U> const &) = delete;
	bool operator== (btcb::store_iterator_impl<T, U> const * other_a) const
	{
		return (other_a != nullptr && *this == *other_a) || (other_a == nullptr && is_end_sentinal ());
	}
	bool operator!= (btcb::store_iterator_impl<T, U> const & other_a) const
	{
		return !(*this == other_a);
	}
};
/**
 * Iterates the key/value pairs of a transaction
 */
template <typename T, typename U>
class store_iterator
{
public:
	store_iterator (std::nullptr_t)
	{
	}
	store_iterator (std::unique_ptr<btcb::store_iterator_impl<T, U>> impl_a) :
	impl (std::move (impl_a))
	{
		impl->fill (current);
	}
	store_iterator (btcb::store_iterator<T, U> && other_a) :
	current (std::move (other_a.current)),
	impl (std::move (other_a.impl))
	{
	}
	btcb::store_iterator<T, U> & operator++ ()
	{
		++*impl;
		impl->fill (current);
		return *this;
	}
	btcb::store_iterator<T, U> & operator= (btcb::store_iterator<T, U> && other_a)
	{
		impl = std::move (other_a.impl);
		current = std::move (other_a.current);
		return *this;
	}
	btcb::store_iterator<T, U> & operator= (btcb::store_iterator<T, U> const &) = delete;
	std::pair<T, U> * operator-> ()
	{
		return &current;
	}
	bool operator== (btcb::store_iterator<T, U> const & other_a) const
	{
		return (impl == nullptr && other_a.impl == nullptr) || (impl != nullptr && *impl == other_a.impl.get ()) || (other_a.impl != nullptr && *other_a.impl == impl.get ());
	}
	bool operator!= (btcb::store_iterator<T, U> const & other_a) const
	{
		return !(*this == other_a);
	}

private:
	std::pair<T, U> current;
	std::unique_ptr<btcb::store_iterator_impl<T, U>> impl;
};

class block_predecessor_set;

class transaction_impl
{
public:
	virtual ~transaction_impl () = default;
};
/**
 * RAII wrapper of MDB_txn where the constructor starts the transaction
 * and the destructor commits it.
 */
class transaction
{
public:
	std::unique_ptr<btcb::transaction_impl> impl;
};

/**
 * Manages block storage and iteration
 */
class block_store
{
public:
	virtual ~block_store () = default;
	virtual void initialize (btcb::transaction const &, btcb::genesis const &) = 0;
	virtual void block_put (btcb::transaction const &, btcb::block_hash const &, btcb::block const &, btcb::block_hash const & = btcb::block_hash (0), btcb::epoch version = btcb::epoch::epoch_0) = 0;
	virtual btcb::block_hash block_successor (btcb::transaction const &, btcb::block_hash const &) = 0;
	virtual void block_successor_clear (btcb::transaction const &, btcb::block_hash const &) = 0;
	virtual std::shared_ptr<btcb::block> block_get (btcb::transaction const &, btcb::block_hash const &) = 0;
	virtual std::shared_ptr<btcb::block> block_random (btcb::transaction const &) = 0;
	virtual void block_del (btcb::transaction const &, btcb::block_hash const &) = 0;
	virtual bool block_exists (btcb::transaction const &, btcb::block_hash const &) = 0;
	virtual bool block_exists (btcb::transaction const &, btcb::block_type, btcb::block_hash const &) = 0;
	virtual btcb::block_counts block_count (btcb::transaction const &) = 0;
	virtual bool root_exists (btcb::transaction const &, btcb::uint256_union const &) = 0;

	virtual void frontier_put (btcb::transaction const &, btcb::block_hash const &, btcb::account const &) = 0;
	virtual btcb::account frontier_get (btcb::transaction const &, btcb::block_hash const &) = 0;
	virtual void frontier_del (btcb::transaction const &, btcb::block_hash const &) = 0;

	virtual void account_put (btcb::transaction const &, btcb::account const &, btcb::account_info const &) = 0;
	virtual bool account_get (btcb::transaction const &, btcb::account const &, btcb::account_info &) = 0;
	virtual void account_del (btcb::transaction const &, btcb::account const &) = 0;
	virtual bool account_exists (btcb::transaction const &, btcb::account const &) = 0;
	virtual size_t account_count (btcb::transaction const &) = 0;
	virtual btcb::store_iterator<btcb::account, btcb::account_info> latest_v0_begin (btcb::transaction const &, btcb::account const &) = 0;
	virtual btcb::store_iterator<btcb::account, btcb::account_info> latest_v0_begin (btcb::transaction const &) = 0;
	virtual btcb::store_iterator<btcb::account, btcb::account_info> latest_v0_end () = 0;
	virtual btcb::store_iterator<btcb::account, btcb::account_info> latest_v1_begin (btcb::transaction const &, btcb::account const &) = 0;
	virtual btcb::store_iterator<btcb::account, btcb::account_info> latest_v1_begin (btcb::transaction const &) = 0;
	virtual btcb::store_iterator<btcb::account, btcb::account_info> latest_v1_end () = 0;
	virtual btcb::store_iterator<btcb::account, btcb::account_info> latest_begin (btcb::transaction const &, btcb::account const &) = 0;
	virtual btcb::store_iterator<btcb::account, btcb::account_info> latest_begin (btcb::transaction const &) = 0;
	virtual btcb::store_iterator<btcb::account, btcb::account_info> latest_end () = 0;

	virtual void pending_put (btcb::transaction const &, btcb::pending_key const &, btcb::pending_info const &) = 0;
	virtual void pending_del (btcb::transaction const &, btcb::pending_key const &) = 0;
	virtual bool pending_get (btcb::transaction const &, btcb::pending_key const &, btcb::pending_info &) = 0;
	virtual bool pending_exists (btcb::transaction const &, btcb::pending_key const &) = 0;
	virtual btcb::store_iterator<btcb::pending_key, btcb::pending_info> pending_v0_begin (btcb::transaction const &, btcb::pending_key const &) = 0;
	virtual btcb::store_iterator<btcb::pending_key, btcb::pending_info> pending_v0_begin (btcb::transaction const &) = 0;
	virtual btcb::store_iterator<btcb::pending_key, btcb::pending_info> pending_v0_end () = 0;
	virtual btcb::store_iterator<btcb::pending_key, btcb::pending_info> pending_v1_begin (btcb::transaction const &, btcb::pending_key const &) = 0;
	virtual btcb::store_iterator<btcb::pending_key, btcb::pending_info> pending_v1_begin (btcb::transaction const &) = 0;
	virtual btcb::store_iterator<btcb::pending_key, btcb::pending_info> pending_v1_end () = 0;
	virtual btcb::store_iterator<btcb::pending_key, btcb::pending_info> pending_begin (btcb::transaction const &, btcb::pending_key const &) = 0;
	virtual btcb::store_iterator<btcb::pending_key, btcb::pending_info> pending_begin (btcb::transaction const &) = 0;
	virtual btcb::store_iterator<btcb::pending_key, btcb::pending_info> pending_end () = 0;

	virtual void block_info_put (btcb::transaction const &, btcb::block_hash const &, btcb::block_info const &) = 0;
	virtual void block_info_del (btcb::transaction const &, btcb::block_hash const &) = 0;
	virtual bool block_info_get (btcb::transaction const &, btcb::block_hash const &, btcb::block_info &) = 0;
	virtual bool block_info_exists (btcb::transaction const &, btcb::block_hash const &) = 0;
	virtual btcb::store_iterator<btcb::block_hash, btcb::block_info> block_info_begin (btcb::transaction const &, btcb::block_hash const &) = 0;
	virtual btcb::store_iterator<btcb::block_hash, btcb::block_info> block_info_begin (btcb::transaction const &) = 0;
	virtual btcb::store_iterator<btcb::block_hash, btcb::block_info> block_info_end () = 0;
	virtual btcb::uint128_t block_balance (btcb::transaction const &, btcb::block_hash const &) = 0;
	virtual btcb::epoch block_version (btcb::transaction const &, btcb::block_hash const &) = 0;
	static size_t const block_info_max = 32;

	virtual btcb::uint128_t representation_get (btcb::transaction const &, btcb::account const &) = 0;
	virtual void representation_put (btcb::transaction const &, btcb::account const &, btcb::uint128_t const &) = 0;
	virtual void representation_add (btcb::transaction const &, btcb::account const &, btcb::uint128_t const &) = 0;
	virtual btcb::store_iterator<btcb::account, btcb::uint128_union> representation_begin (btcb::transaction const &) = 0;
	virtual btcb::store_iterator<btcb::account, btcb::uint128_union> representation_end () = 0;

	virtual void unchecked_clear (btcb::transaction const &) = 0;
	virtual void unchecked_put (btcb::transaction const &, btcb::unchecked_key const &, std::shared_ptr<btcb::block> const &) = 0;
	virtual void unchecked_put (btcb::transaction const &, btcb::block_hash const &, std::shared_ptr<btcb::block> const &) = 0;
	virtual std::vector<std::shared_ptr<btcb::block>> unchecked_get (btcb::transaction const &, btcb::block_hash const &) = 0;
	virtual bool unchecked_exists (btcb::transaction const &, btcb::unchecked_key const &) = 0;
	virtual void unchecked_del (btcb::transaction const &, btcb::unchecked_key const &) = 0;
	virtual btcb::store_iterator<btcb::unchecked_key, std::shared_ptr<btcb::block>> unchecked_begin (btcb::transaction const &) = 0;
	virtual btcb::store_iterator<btcb::unchecked_key, std::shared_ptr<btcb::block>> unchecked_begin (btcb::transaction const &, btcb::unchecked_key const &) = 0;
	virtual btcb::store_iterator<btcb::unchecked_key, std::shared_ptr<btcb::block>> unchecked_end () = 0;
	virtual size_t unchecked_count (btcb::transaction const &) = 0;

	virtual void checksum_put (btcb::transaction const &, uint64_t, uint8_t, btcb::checksum const &) = 0;
	virtual bool checksum_get (btcb::transaction const &, uint64_t, uint8_t, btcb::checksum &) = 0;
	virtual void checksum_del (btcb::transaction const &, uint64_t, uint8_t) = 0;

	// Return latest vote for an account from store
	virtual std::shared_ptr<btcb::vote> vote_get (btcb::transaction const &, btcb::account const &) = 0;
	// Populate vote with the next sequence number
	virtual std::shared_ptr<btcb::vote> vote_generate (btcb::transaction const &, btcb::account const &, btcb::raw_key const &, std::shared_ptr<btcb::block>) = 0;
	virtual std::shared_ptr<btcb::vote> vote_generate (btcb::transaction const &, btcb::account const &, btcb::raw_key const &, std::vector<btcb::block_hash>) = 0;
	// Return either vote or the stored vote with a higher sequence number
	virtual std::shared_ptr<btcb::vote> vote_max (btcb::transaction const &, std::shared_ptr<btcb::vote>) = 0;
	// Return latest vote for an account considering the vote cache
	virtual std::shared_ptr<btcb::vote> vote_current (btcb::transaction const &, btcb::account const &) = 0;
	virtual void flush (btcb::transaction const &) = 0;
	virtual btcb::store_iterator<btcb::account, std::shared_ptr<btcb::vote>> vote_begin (btcb::transaction const &) = 0;
	virtual btcb::store_iterator<btcb::account, std::shared_ptr<btcb::vote>> vote_end () = 0;

	virtual void version_put (btcb::transaction const &, int) = 0;
	virtual int version_get (btcb::transaction const &) = 0;

	// Requires a write transaction
	virtual btcb::raw_key get_node_id (btcb::transaction const &) = 0;

	/** Deletes the node ID from the store */
	virtual void delete_node_id (btcb::transaction const &) = 0;

	/** Start read-write transaction */
	virtual btcb::transaction tx_begin_write () = 0;

	/** Start read-only transaction */
	virtual btcb::transaction tx_begin_read () = 0;

	/**
	 * Start a read-only or read-write transaction
	 * @param write If true, start a read-write transaction
	 */
	virtual btcb::transaction tx_begin (bool write = false) = 0;
};
}
