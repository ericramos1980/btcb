#include <btcb/node/lmdb.hpp>

#include <btcb/lib/utility.hpp>
#include <btcb/node/common.hpp>
#include <btcb/secure/versioning.hpp>

#include <boost/polymorphic_cast.hpp>

#include <queue>

btcb::mdb_env::mdb_env (bool & error_a, boost::filesystem::path const & path_a, int max_dbs)
{
	boost::system::error_code error_mkdir, error_chmod;
	if (path_a.has_parent_path ())
	{
		boost::filesystem::create_directories (path_a.parent_path (), error_mkdir);
		btcb::set_secure_perm_directory (path_a.parent_path (), error_chmod);
		if (!error_mkdir)
		{
			auto status1 (mdb_env_create (&environment));
			release_assert (status1 == 0);
			auto status2 (mdb_env_set_maxdbs (environment, max_dbs));
			release_assert (status2 == 0);
			auto status3 (mdb_env_set_mapsize (environment, 1ULL * 1024 * 1024 * 1024 * 128)); // 128 Gigabyte
			release_assert (status3 == 0);
			// It seems if there's ever more threads than mdb_env_set_maxreaders has read slots available, we get failures on transaction creation unless MDB_NOTLS is specified
			// This can happen if something like 256 io_threads are specified in the node config
			auto status4 (mdb_env_open (environment, path_a.string ().c_str (), MDB_NOSUBDIR | MDB_NOTLS, 00600));
			release_assert (status4 == 0);
			error_a = status4 != 0;
		}
		else
		{
			error_a = true;
			environment = nullptr;
		}
	}
	else
	{
		error_a = true;
		environment = nullptr;
	}
}

btcb::mdb_env::~mdb_env ()
{
	if (environment != nullptr)
	{
		mdb_env_close (environment);
	}
}

btcb::mdb_env::operator MDB_env * () const
{
	return environment;
}

btcb::transaction btcb::mdb_env::tx_begin (bool write_a) const
{
	return { std::make_unique<btcb::mdb_txn> (*this, write_a) };
}

MDB_txn * btcb::mdb_env::tx (btcb::transaction const & transaction_a) const
{
	auto result (boost::polymorphic_downcast<btcb::mdb_txn *> (transaction_a.impl.get ()));
	release_assert (mdb_txn_env (result->handle) == environment);
	return *result;
}

btcb::mdb_val::mdb_val (btcb::epoch epoch_a) :
value ({ 0, nullptr }),
epoch (epoch_a)
{
}

btcb::mdb_val::mdb_val (MDB_val const & value_a, btcb::epoch epoch_a) :
value (value_a),
epoch (epoch_a)
{
}

btcb::mdb_val::mdb_val (size_t size_a, void * data_a) :
value ({ size_a, data_a })
{
}

btcb::mdb_val::mdb_val (btcb::uint128_union const & val_a) :
mdb_val (sizeof (val_a), const_cast<btcb::uint128_union *> (&val_a))
{
}

btcb::mdb_val::mdb_val (btcb::uint256_union const & val_a) :
mdb_val (sizeof (val_a), const_cast<btcb::uint256_union *> (&val_a))
{
}

btcb::mdb_val::mdb_val (btcb::account_info const & val_a) :
mdb_val (val_a.db_size (), const_cast<btcb::account_info *> (&val_a))
{
}

btcb::mdb_val::mdb_val (btcb::pending_info const & val_a) :
mdb_val (sizeof (val_a.source) + sizeof (val_a.amount), const_cast<btcb::pending_info *> (&val_a))
{
}

btcb::mdb_val::mdb_val (btcb::pending_key const & val_a) :
mdb_val (sizeof (val_a), const_cast<btcb::pending_key *> (&val_a))
{
}

btcb::mdb_val::mdb_val (btcb::block_info const & val_a) :
mdb_val (sizeof (val_a), const_cast<btcb::block_info *> (&val_a))
{
}

btcb::mdb_val::mdb_val (std::shared_ptr<btcb::block> const & val_a) :
buffer (std::make_shared<std::vector<uint8_t>> ())
{
	{
		btcb::vectorstream stream (*buffer);
		btcb::serialize_block (stream, *val_a);
	}
	value = { buffer->size (), const_cast<uint8_t *> (buffer->data ()) };
}

void * btcb::mdb_val::data () const
{
	return value.mv_data;
}

size_t btcb::mdb_val::size () const
{
	return value.mv_size;
}

btcb::mdb_val::operator btcb::account_info () const
{
	btcb::account_info result;
	result.epoch = epoch;
	assert (value.mv_size == result.db_size ());
	std::copy (reinterpret_cast<uint8_t const *> (value.mv_data), reinterpret_cast<uint8_t const *> (value.mv_data) + result.db_size (), reinterpret_cast<uint8_t *> (&result));
	return result;
}

btcb::mdb_val::operator btcb::block_info () const
{
	btcb::block_info result;
	assert (value.mv_size == sizeof (result));
	static_assert (sizeof (btcb::block_info::account) + sizeof (btcb::block_info::balance) == sizeof (result), "Packed class");
	std::copy (reinterpret_cast<uint8_t const *> (value.mv_data), reinterpret_cast<uint8_t const *> (value.mv_data) + sizeof (result), reinterpret_cast<uint8_t *> (&result));
	return result;
}

btcb::mdb_val::operator btcb::pending_info () const
{
	btcb::pending_info result;
	result.epoch = epoch;
	std::copy (reinterpret_cast<uint8_t const *> (value.mv_data), reinterpret_cast<uint8_t const *> (value.mv_data) + sizeof (btcb::pending_info::source) + sizeof (btcb::pending_info::amount), reinterpret_cast<uint8_t *> (&result));
	return result;
}

btcb::mdb_val::operator btcb::pending_key () const
{
	btcb::pending_key result;
	assert (value.mv_size == sizeof (result));
	static_assert (sizeof (btcb::pending_key::account) + sizeof (btcb::pending_key::hash) == sizeof (result), "Packed class");
	std::copy (reinterpret_cast<uint8_t const *> (value.mv_data), reinterpret_cast<uint8_t const *> (value.mv_data) + sizeof (result), reinterpret_cast<uint8_t *> (&result));
	return result;
}

btcb::mdb_val::operator btcb::uint128_union () const
{
	btcb::uint128_union result;
	assert (size () == sizeof (result));
	std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), result.bytes.data ());
	return result;
}

btcb::mdb_val::operator btcb::uint256_union () const
{
	btcb::uint256_union result;
	assert (size () == sizeof (result));
	std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), result.bytes.data ());
	return result;
}

btcb::mdb_val::operator std::array<char, 64> () const
{
	btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.mv_data), value.mv_size);
	std::array<char, 64> result;
	btcb::read (stream, result);
	return result;
}

btcb::mdb_val::operator no_value () const
{
	return no_value::dummy;
}

btcb::mdb_val::operator std::shared_ptr<btcb::block> () const
{
	btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.mv_data), value.mv_size);
	std::shared_ptr<btcb::block> result (btcb::deserialize_block (stream));
	return result;
}

btcb::mdb_val::operator std::shared_ptr<btcb::send_block> () const
{
	btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.mv_data), value.mv_size);
	auto error (false);
	std::shared_ptr<btcb::send_block> result (std::make_shared<btcb::send_block> (error, stream));
	assert (!error);
	return result;
}

btcb::mdb_val::operator std::shared_ptr<btcb::receive_block> () const
{
	btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.mv_data), value.mv_size);
	auto error (false);
	std::shared_ptr<btcb::receive_block> result (std::make_shared<btcb::receive_block> (error, stream));
	assert (!error);
	return result;
}

btcb::mdb_val::operator std::shared_ptr<btcb::open_block> () const
{
	btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.mv_data), value.mv_size);
	auto error (false);
	std::shared_ptr<btcb::open_block> result (std::make_shared<btcb::open_block> (error, stream));
	assert (!error);
	return result;
}

btcb::mdb_val::operator std::shared_ptr<btcb::change_block> () const
{
	btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.mv_data), value.mv_size);
	auto error (false);
	std::shared_ptr<btcb::change_block> result (std::make_shared<btcb::change_block> (error, stream));
	assert (!error);
	return result;
}

btcb::mdb_val::operator std::shared_ptr<btcb::state_block> () const
{
	btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.mv_data), value.mv_size);
	auto error (false);
	std::shared_ptr<btcb::state_block> result (std::make_shared<btcb::state_block> (error, stream));
	assert (!error);
	return result;
}

btcb::mdb_val::operator std::shared_ptr<btcb::vote> () const
{
	btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.mv_data), value.mv_size);
	auto error (false);
	std::shared_ptr<btcb::vote> result (std::make_shared<btcb::vote> (error, stream));
	assert (!error);
	return result;
}

btcb::mdb_val::operator uint64_t () const
{
	uint64_t result;
	btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.mv_data), value.mv_size);
	auto error (btcb::read (stream, result));
	assert (!error);
	return result;
}

btcb::mdb_val::operator MDB_val * () const
{
	// Allow passing a temporary to a non-c++ function which doesn't have constness
	return const_cast<MDB_val *> (&value);
};

btcb::mdb_val::operator MDB_val const & () const
{
	return value;
}

btcb::mdb_txn::mdb_txn (btcb::mdb_env const & environment_a, bool write_a)
{
	auto status (mdb_txn_begin (environment_a, nullptr, write_a ? 0 : MDB_RDONLY, &handle));
	release_assert (status == 0);
}

btcb::mdb_txn::~mdb_txn ()
{
	auto status (mdb_txn_commit (handle));
	release_assert (status == 0);
}

btcb::mdb_txn::operator MDB_txn * () const
{
	return handle;
}

namespace btcb
{
/**
	 * Fill in our predecessors
	 */
class block_predecessor_set : public btcb::block_visitor
{
public:
	block_predecessor_set (btcb::transaction const & transaction_a, btcb::mdb_store & store_a) :
	transaction (transaction_a),
	store (store_a)
	{
	}
	virtual ~block_predecessor_set () = default;
	void fill_value (btcb::block const & block_a)
	{
		auto hash (block_a.hash ());
		btcb::block_type type;
		auto value (store.block_raw_get (transaction, block_a.previous (), type));
		auto version (store.block_version (transaction, block_a.previous ()));
		assert (value.mv_size != 0);
		std::vector<uint8_t> data (static_cast<uint8_t *> (value.mv_data), static_cast<uint8_t *> (value.mv_data) + value.mv_size);
		std::copy (hash.bytes.begin (), hash.bytes.end (), data.end () - hash.bytes.size ());
		store.block_raw_put (transaction, store.block_database (type, version), block_a.previous (), btcb::mdb_val (data.size (), data.data ()));
	}
	void send_block (btcb::send_block const & block_a) override
	{
		fill_value (block_a);
	}
	void receive_block (btcb::receive_block const & block_a) override
	{
		fill_value (block_a);
	}
	void open_block (btcb::open_block const & block_a) override
	{
		// Open blocks don't have a predecessor
	}
	void change_block (btcb::change_block const & block_a) override
	{
		fill_value (block_a);
	}
	void state_block (btcb::state_block const & block_a) override
	{
		if (!block_a.previous ().is_zero ())
		{
			fill_value (block_a);
		}
	}
	btcb::transaction const & transaction;
	btcb::mdb_store & store;
};
}

template <typename T, typename U>
btcb::mdb_iterator<T, U>::mdb_iterator (btcb::transaction const & transaction_a, MDB_dbi db_a, btcb::epoch epoch_a) :
cursor (nullptr)
{
	current.first.epoch = epoch_a;
	current.second.epoch = epoch_a;
	auto status (mdb_cursor_open (tx (transaction_a), db_a, &cursor));
	release_assert (status == 0);
	auto status2 (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_FIRST));
	release_assert (status2 == 0 || status2 == MDB_NOTFOUND);
	if (status2 != MDB_NOTFOUND)
	{
		auto status3 (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_GET_CURRENT));
		release_assert (status3 == 0 || status3 == MDB_NOTFOUND);
		if (current.first.size () != sizeof (T))
		{
			clear ();
		}
	}
	else
	{
		clear ();
	}
}

template <typename T, typename U>
btcb::mdb_iterator<T, U>::mdb_iterator (std::nullptr_t, btcb::epoch epoch_a) :
cursor (nullptr)
{
	current.first.epoch = epoch_a;
	current.second.epoch = epoch_a;
}

template <typename T, typename U>
btcb::mdb_iterator<T, U>::mdb_iterator (btcb::transaction const & transaction_a, MDB_dbi db_a, MDB_val const & val_a, btcb::epoch epoch_a) :
cursor (nullptr)
{
	current.first.epoch = epoch_a;
	current.second.epoch = epoch_a;
	auto status (mdb_cursor_open (tx (transaction_a), db_a, &cursor));
	release_assert (status == 0);
	current.first = val_a;
	auto status2 (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_SET_RANGE));
	release_assert (status2 == 0 || status2 == MDB_NOTFOUND);
	if (status2 != MDB_NOTFOUND)
	{
		auto status3 (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_GET_CURRENT));
		release_assert (status3 == 0 || status3 == MDB_NOTFOUND);
		if (current.first.size () != sizeof (T))
		{
			clear ();
		}
	}
	else
	{
		clear ();
	}
}

template <typename T, typename U>
btcb::mdb_iterator<T, U>::mdb_iterator (btcb::mdb_iterator<T, U> && other_a)
{
	cursor = other_a.cursor;
	other_a.cursor = nullptr;
	current = other_a.current;
}

template <typename T, typename U>
btcb::mdb_iterator<T, U>::~mdb_iterator ()
{
	if (cursor != nullptr)
	{
		mdb_cursor_close (cursor);
	}
}

template <typename T, typename U>
btcb::store_iterator_impl<T, U> & btcb::mdb_iterator<T, U>::operator++ ()
{
	assert (cursor != nullptr);
	auto status (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_NEXT));
	release_assert (status == 0 || status == MDB_NOTFOUND);
	if (status == MDB_NOTFOUND)
	{
		clear ();
	}
	if (current.first.size () != sizeof (T))
	{
		clear ();
	}
	return *this;
}

template <typename T, typename U>
btcb::mdb_iterator<T, U> & btcb::mdb_iterator<T, U>::operator= (btcb::mdb_iterator<T, U> && other_a)
{
	if (cursor != nullptr)
	{
		mdb_cursor_close (cursor);
	}
	cursor = other_a.cursor;
	other_a.cursor = nullptr;
	current = other_a.current;
	other_a.clear ();
	return *this;
}

template <typename T, typename U>
std::pair<btcb::mdb_val, btcb::mdb_val> * btcb::mdb_iterator<T, U>::operator-> ()
{
	return &current;
}

template <typename T, typename U>
bool btcb::mdb_iterator<T, U>::operator== (btcb::store_iterator_impl<T, U> const & base_a) const
{
	auto const other_a (boost::polymorphic_downcast<btcb::mdb_iterator<T, U> const *> (&base_a));
	auto result (current.first.data () == other_a->current.first.data ());
	assert (!result || (current.first.size () == other_a->current.first.size ()));
	assert (!result || (current.second.data () == other_a->current.second.data ()));
	assert (!result || (current.second.size () == other_a->current.second.size ()));
	return result;
}

template <typename T, typename U>
void btcb::mdb_iterator<T, U>::clear ()
{
	current.first = btcb::mdb_val (current.first.epoch);
	current.second = btcb::mdb_val (current.second.epoch);
	assert (is_end_sentinal ());
}

template <typename T, typename U>
MDB_txn * btcb::mdb_iterator<T, U>::tx (btcb::transaction const & transaction_a) const
{
	auto result (boost::polymorphic_downcast<btcb::mdb_txn *> (transaction_a.impl.get ()));
	return *result;
}

template <typename T, typename U>
bool btcb::mdb_iterator<T, U>::is_end_sentinal () const
{
	return current.first.size () == 0;
}

template <typename T, typename U>
void btcb::mdb_iterator<T, U>::fill (std::pair<T, U> & value_a) const
{
	if (current.first.size () != 0)
	{
		value_a.first = static_cast<T> (current.first);
	}
	else
	{
		value_a.first = T ();
	}
	if (current.second.size () != 0)
	{
		value_a.second = static_cast<U> (current.second);
	}
	else
	{
		value_a.second = U ();
	}
}

template <typename T, typename U>
std::pair<btcb::mdb_val, btcb::mdb_val> * btcb::mdb_merge_iterator<T, U>::operator-> ()
{
	return least_iterator ().operator-> ();
}

template <typename T, typename U>
btcb::mdb_merge_iterator<T, U>::mdb_merge_iterator (btcb::transaction const & transaction_a, MDB_dbi db1_a, MDB_dbi db2_a) :
impl1 (std::make_unique<btcb::mdb_iterator<T, U>> (transaction_a, db1_a, btcb::epoch::epoch_0)),
impl2 (std::make_unique<btcb::mdb_iterator<T, U>> (transaction_a, db2_a, btcb::epoch::epoch_1))
{
}

template <typename T, typename U>
btcb::mdb_merge_iterator<T, U>::mdb_merge_iterator (std::nullptr_t) :
impl1 (std::make_unique<btcb::mdb_iterator<T, U>> (nullptr, btcb::epoch::epoch_0)),
impl2 (std::make_unique<btcb::mdb_iterator<T, U>> (nullptr, btcb::epoch::epoch_1))
{
}

template <typename T, typename U>
btcb::mdb_merge_iterator<T, U>::mdb_merge_iterator (btcb::transaction const & transaction_a, MDB_dbi db1_a, MDB_dbi db2_a, MDB_val const & val_a) :
impl1 (std::make_unique<btcb::mdb_iterator<T, U>> (transaction_a, db1_a, val_a, btcb::epoch::epoch_0)),
impl2 (std::make_unique<btcb::mdb_iterator<T, U>> (transaction_a, db2_a, val_a, btcb::epoch::epoch_1))
{
}

template <typename T, typename U>
btcb::mdb_merge_iterator<T, U>::mdb_merge_iterator (btcb::mdb_merge_iterator<T, U> && other_a)
{
	impl1 = std::move (other_a.impl1);
	impl2 = std::move (other_a.impl2);
}

template <typename T, typename U>
btcb::mdb_merge_iterator<T, U>::~mdb_merge_iterator ()
{
}

template <typename T, typename U>
btcb::store_iterator_impl<T, U> & btcb::mdb_merge_iterator<T, U>::operator++ ()
{
	++least_iterator ();
	return *this;
}

template <typename T, typename U>
bool btcb::mdb_merge_iterator<T, U>::is_end_sentinal () const
{
	return least_iterator ().is_end_sentinal ();
}

template <typename T, typename U>
void btcb::mdb_merge_iterator<T, U>::fill (std::pair<T, U> & value_a) const
{
	auto & current (least_iterator ());
	if (current->first.size () != 0)
	{
		value_a.first = static_cast<T> (current->first);
	}
	else
	{
		value_a.first = T ();
	}
	if (current->second.size () != 0)
	{
		value_a.second = static_cast<U> (current->second);
	}
	else
	{
		value_a.second = U ();
	}
}

template <typename T, typename U>
bool btcb::mdb_merge_iterator<T, U>::operator== (btcb::store_iterator_impl<T, U> const & base_a) const
{
	assert ((dynamic_cast<btcb::mdb_merge_iterator<T, U> const *> (&base_a) != nullptr) && "Incompatible iterator comparison");
	auto & other (static_cast<btcb::mdb_merge_iterator<T, U> const &> (base_a));
	return *impl1 == *other.impl1 && *impl2 == *other.impl2;
}

template <typename T, typename U>
btcb::mdb_iterator<T, U> & btcb::mdb_merge_iterator<T, U>::least_iterator () const
{
	btcb::mdb_iterator<T, U> * result;
	if (impl1->is_end_sentinal ())
	{
		result = impl2.get ();
	}
	else if (impl2->is_end_sentinal ())
	{
		result = impl1.get ();
	}
	else
	{
		auto key_cmp (mdb_cmp (mdb_cursor_txn (impl1->cursor), mdb_cursor_dbi (impl1->cursor), impl1->current.first, impl2->current.first));

		if (key_cmp < 0)
		{
			result = impl1.get ();
		}
		else if (key_cmp > 0)
		{
			result = impl2.get ();
		}
		else
		{
			auto val_cmp (mdb_cmp (mdb_cursor_txn (impl1->cursor), mdb_cursor_dbi (impl1->cursor), impl1->current.second, impl2->current.second));
			result = val_cmp < 0 ? impl1.get () : impl2.get ();
		}
	}
	return *result;
}

btcb::wallet_value::wallet_value (btcb::mdb_val const & val_a)
{
	assert (val_a.size () == sizeof (*this));
	std::copy (reinterpret_cast<uint8_t const *> (val_a.data ()), reinterpret_cast<uint8_t const *> (val_a.data ()) + sizeof (key), key.chars.begin ());
	std::copy (reinterpret_cast<uint8_t const *> (val_a.data ()) + sizeof (key), reinterpret_cast<uint8_t const *> (val_a.data ()) + sizeof (key) + sizeof (work), reinterpret_cast<char *> (&work));
}

btcb::wallet_value::wallet_value (btcb::uint256_union const & key_a, uint64_t work_a) :
key (key_a),
work (work_a)
{
}

btcb::mdb_val btcb::wallet_value::val () const
{
	static_assert (sizeof (*this) == sizeof (key) + sizeof (work), "Class not packed");
	return btcb::mdb_val (sizeof (*this), const_cast<btcb::wallet_value *> (this));
}

template class btcb::mdb_iterator<btcb::pending_key, btcb::pending_info>;
template class btcb::mdb_iterator<btcb::uint256_union, btcb::block_info>;
template class btcb::mdb_iterator<btcb::uint256_union, btcb::uint128_union>;
template class btcb::mdb_iterator<btcb::uint256_union, btcb::uint256_union>;
template class btcb::mdb_iterator<btcb::uint256_union, std::shared_ptr<btcb::block>>;
template class btcb::mdb_iterator<btcb::uint256_union, std::shared_ptr<btcb::vote>>;
template class btcb::mdb_iterator<btcb::uint256_union, btcb::wallet_value>;
template class btcb::mdb_iterator<std::array<char, 64>, btcb::mdb_val::no_value>;

btcb::store_iterator<btcb::block_hash, btcb::block_info> btcb::mdb_store::block_info_begin (btcb::transaction const & transaction_a, btcb::block_hash const & hash_a)
{
	btcb::store_iterator<btcb::block_hash, btcb::block_info> result (std::make_unique<btcb::mdb_iterator<btcb::block_hash, btcb::block_info>> (transaction_a, blocks_info, btcb::mdb_val (hash_a)));
	return result;
}

btcb::store_iterator<btcb::block_hash, btcb::block_info> btcb::mdb_store::block_info_begin (btcb::transaction const & transaction_a)
{
	btcb::store_iterator<btcb::block_hash, btcb::block_info> result (std::make_unique<btcb::mdb_iterator<btcb::block_hash, btcb::block_info>> (transaction_a, blocks_info));
	return result;
}

btcb::store_iterator<btcb::block_hash, btcb::block_info> btcb::mdb_store::block_info_end ()
{
	btcb::store_iterator<btcb::block_hash, btcb::block_info> result (nullptr);
	return result;
}

btcb::store_iterator<btcb::account, btcb::uint128_union> btcb::mdb_store::representation_begin (btcb::transaction const & transaction_a)
{
	btcb::store_iterator<btcb::account, btcb::uint128_union> result (std::make_unique<btcb::mdb_iterator<btcb::account, btcb::uint128_union>> (transaction_a, representation));
	return result;
}

btcb::store_iterator<btcb::account, btcb::uint128_union> btcb::mdb_store::representation_end ()
{
	btcb::store_iterator<btcb::account, btcb::uint128_union> result (nullptr);
	return result;
}

btcb::store_iterator<btcb::unchecked_key, std::shared_ptr<btcb::block>> btcb::mdb_store::unchecked_begin (btcb::transaction const & transaction_a)
{
	btcb::store_iterator<btcb::unchecked_key, std::shared_ptr<btcb::block>> result (std::make_unique<btcb::mdb_iterator<btcb::unchecked_key, std::shared_ptr<btcb::block>>> (transaction_a, unchecked));
	return result;
}

btcb::store_iterator<btcb::unchecked_key, std::shared_ptr<btcb::block>> btcb::mdb_store::unchecked_begin (btcb::transaction const & transaction_a, btcb::unchecked_key const & key_a)
{
	btcb::store_iterator<btcb::unchecked_key, std::shared_ptr<btcb::block>> result (std::make_unique<btcb::mdb_iterator<btcb::unchecked_key, std::shared_ptr<btcb::block>>> (transaction_a, unchecked, btcb::mdb_val (key_a)));
	return result;
}

btcb::store_iterator<btcb::unchecked_key, std::shared_ptr<btcb::block>> btcb::mdb_store::unchecked_end ()
{
	btcb::store_iterator<btcb::unchecked_key, std::shared_ptr<btcb::block>> result (nullptr);
	return result;
}

btcb::store_iterator<btcb::account, std::shared_ptr<btcb::vote>> btcb::mdb_store::vote_begin (btcb::transaction const & transaction_a)
{
	return btcb::store_iterator<btcb::account, std::shared_ptr<btcb::vote>> (std::make_unique<btcb::mdb_iterator<btcb::account, std::shared_ptr<btcb::vote>>> (transaction_a, vote));
}

btcb::store_iterator<btcb::account, std::shared_ptr<btcb::vote>> btcb::mdb_store::vote_end ()
{
	return btcb::store_iterator<btcb::account, std::shared_ptr<btcb::vote>> (nullptr);
}

btcb::mdb_store::mdb_store (bool & error_a, boost::filesystem::path const & path_a, int lmdb_max_dbs) :
env (error_a, path_a, lmdb_max_dbs),
frontiers (0),
accounts_v0 (0),
accounts_v1 (0),
send_blocks (0),
receive_blocks (0),
open_blocks (0),
change_blocks (0),
state_blocks_v0 (0),
state_blocks_v1 (0),
pending_v0 (0),
pending_v1 (0),
blocks_info (0),
representation (0),
unchecked (0),
checksum (0),
vote (0),
meta (0)
{
	if (!error_a)
	{
		auto transaction (tx_begin_write ());
		error_a |= mdb_dbi_open (env.tx (transaction), "frontiers", MDB_CREATE, &frontiers) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "accounts", MDB_CREATE, &accounts_v0) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "accounts_v1", MDB_CREATE, &accounts_v1) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "send", MDB_CREATE, &send_blocks) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "receive", MDB_CREATE, &receive_blocks) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "open", MDB_CREATE, &open_blocks) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "change", MDB_CREATE, &change_blocks) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "state", MDB_CREATE, &state_blocks_v0) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "state_v1", MDB_CREATE, &state_blocks_v1) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "pending", MDB_CREATE, &pending_v0) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "pending_v1", MDB_CREATE, &pending_v1) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "blocks_info", MDB_CREATE, &blocks_info) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "representation", MDB_CREATE, &representation) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "unchecked", MDB_CREATE, &unchecked) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "checksum", MDB_CREATE, &checksum) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "vote", MDB_CREATE, &vote) != 0;
		error_a |= mdb_dbi_open (env.tx (transaction), "meta", MDB_CREATE, &meta) != 0;
		if (!error_a)
		{
			do_upgrades (transaction);
			checksum_put (transaction, 0, 0, 0);
		}
	}
}

btcb::transaction btcb::mdb_store::tx_begin_write ()
{
	return tx_begin (true);
}

btcb::transaction btcb::mdb_store::tx_begin_read ()
{
	return tx_begin (false);
}

btcb::transaction btcb::mdb_store::tx_begin (bool write_a)
{
	return env.tx_begin (write_a);
}

void btcb::mdb_store::initialize (btcb::transaction const & transaction_a, btcb::genesis const & genesis_a)
{
	auto hash_l (genesis_a.hash ());
	assert (latest_v0_begin (transaction_a) == latest_v0_end ());
	assert (latest_v1_begin (transaction_a) == latest_v1_end ());
	block_put (transaction_a, hash_l, *genesis_a.open);
	account_put (transaction_a, genesis_account, { hash_l, genesis_a.open->hash (), genesis_a.open->hash (), std::numeric_limits<btcb::uint128_t>::max (), btcb::seconds_since_epoch (), 1, btcb::epoch::epoch_0 });
	representation_put (transaction_a, genesis_account, std::numeric_limits<btcb::uint128_t>::max ());
	checksum_put (transaction_a, 0, 0, hash_l);
	frontier_put (transaction_a, hash_l, genesis_account);
}

void btcb::mdb_store::version_put (btcb::transaction const & transaction_a, int version_a)
{
	btcb::uint256_union version_key (1);
	btcb::uint256_union version_value (version_a);
	auto status (mdb_put (env.tx (transaction_a), meta, btcb::mdb_val (version_key), btcb::mdb_val (version_value), 0));
	release_assert (status == 0);
}

int btcb::mdb_store::version_get (btcb::transaction const & transaction_a)
{
	btcb::uint256_union version_key (1);
	btcb::mdb_val data;
	auto error (mdb_get (env.tx (transaction_a), meta, btcb::mdb_val (version_key), data));
	int result (1);
	if (error != MDB_NOTFOUND)
	{
		btcb::uint256_union version_value (data);
		assert (version_value.qwords[2] == 0 && version_value.qwords[1] == 0 && version_value.qwords[0] == 0);
		result = version_value.number ().convert_to<int> ();
	}
	return result;
}

btcb::raw_key btcb::mdb_store::get_node_id (btcb::transaction const & transaction_a)
{
	btcb::uint256_union node_id_mdb_key (3);
	btcb::raw_key node_id;
	btcb::mdb_val value;
	auto error (mdb_get (env.tx (transaction_a), meta, btcb::mdb_val (node_id_mdb_key), value));
	if (!error)
	{
		btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
		error = btcb::read (stream, node_id.data);
		assert (!error);
	}
	if (error)
	{
		btcb::random_pool.GenerateBlock (node_id.data.bytes.data (), node_id.data.bytes.size ());
		error = mdb_put (env.tx (transaction_a), meta, btcb::mdb_val (node_id_mdb_key), btcb::mdb_val (node_id.data), 0);
	}
	assert (!error);
	return node_id;
}

void btcb::mdb_store::delete_node_id (btcb::transaction const & transaction_a)
{
	btcb::uint256_union node_id_mdb_key (3);
	auto error (mdb_del (env.tx (transaction_a), meta, btcb::mdb_val (node_id_mdb_key), nullptr));
	assert (!error || error == MDB_NOTFOUND);
}

void btcb::mdb_store::do_upgrades (btcb::transaction const & transaction_a)
{
	switch (version_get (transaction_a))
	{
		case 1:
			upgrade_v1_to_v2 (transaction_a);
		case 2:
			upgrade_v2_to_v3 (transaction_a);
		case 3:
			upgrade_v3_to_v4 (transaction_a);
		case 4:
			upgrade_v4_to_v5 (transaction_a);
		case 5:
			upgrade_v5_to_v6 (transaction_a);
		case 6:
			upgrade_v6_to_v7 (transaction_a);
		case 7:
			upgrade_v7_to_v8 (transaction_a);
		case 8:
			upgrade_v8_to_v9 (transaction_a);
		case 9:
			upgrade_v9_to_v10 (transaction_a);
		case 10:
			upgrade_v10_to_v11 (transaction_a);
		case 11:
			upgrade_v11_to_v12 (transaction_a);
		case 12:
			break;
		default:
			assert (false);
	}
}

void btcb::mdb_store::upgrade_v1_to_v2 (btcb::transaction const & transaction_a)
{
	version_put (transaction_a, 2);
	btcb::account account (1);
	while (!account.is_zero ())
	{
		btcb::mdb_iterator<btcb::uint256_union, btcb::account_info_v1> i (transaction_a, accounts_v0, btcb::mdb_val (account));
		std::cerr << std::hex;
		if (i != btcb::mdb_iterator<btcb::uint256_union, btcb::account_info_v1> (nullptr))
		{
			account = btcb::uint256_union (i->first);
			btcb::account_info_v1 v1 (i->second);
			btcb::account_info_v5 v2;
			v2.balance = v1.balance;
			v2.head = v1.head;
			v2.modified = v1.modified;
			v2.rep_block = v1.rep_block;
			auto block (block_get (transaction_a, v1.head));
			while (!block->previous ().is_zero ())
			{
				block = block_get (transaction_a, block->previous ());
			}
			v2.open_block = block->hash ();
			auto status (mdb_put (env.tx (transaction_a), accounts_v0, btcb::mdb_val (account), v2.val (), 0));
			release_assert (status == 0);
			account = account.number () + 1;
		}
		else
		{
			account.clear ();
		}
	}
}

void btcb::mdb_store::upgrade_v2_to_v3 (btcb::transaction const & transaction_a)
{
	version_put (transaction_a, 3);
	mdb_drop (env.tx (transaction_a), representation, 0);
	for (auto i (std::make_unique<btcb::mdb_iterator<btcb::account, btcb::account_info_v5>> (transaction_a, accounts_v0)), n (std::make_unique<btcb::mdb_iterator<btcb::account, btcb::account_info_v5>> (nullptr)); *i != *n; ++(*i))
	{
		btcb::account account_l ((*i)->first);
		btcb::account_info_v5 info ((*i)->second);
		representative_visitor visitor (transaction_a, *this);
		visitor.compute (info.head);
		assert (!visitor.result.is_zero ());
		info.rep_block = visitor.result;
		auto impl (boost::polymorphic_downcast<btcb::mdb_iterator<btcb::account, btcb::account_info_v5> *> (i.get ()));
		mdb_cursor_put (impl->cursor, btcb::mdb_val (account_l), info.val (), MDB_CURRENT);
		representation_add (transaction_a, visitor.result, info.balance.number ());
	}
}

void btcb::mdb_store::upgrade_v3_to_v4 (btcb::transaction const & transaction_a)
{
	version_put (transaction_a, 4);
	std::queue<std::pair<btcb::pending_key, btcb::pending_info>> items;
	for (auto i (btcb::store_iterator<btcb::block_hash, btcb::pending_info_v3> (std::make_unique<btcb::mdb_iterator<btcb::block_hash, btcb::pending_info_v3>> (transaction_a, pending_v0))), n (btcb::store_iterator<btcb::block_hash, btcb::pending_info_v3> (nullptr)); i != n; ++i)
	{
		btcb::block_hash hash (i->first);
		btcb::pending_info_v3 info (i->second);
		items.push (std::make_pair (btcb::pending_key (info.destination, hash), btcb::pending_info (info.source, info.amount, btcb::epoch::epoch_0)));
	}
	mdb_drop (env.tx (transaction_a), pending_v0, 0);
	while (!items.empty ())
	{
		pending_put (transaction_a, items.front ().first, items.front ().second);
		items.pop ();
	}
}

void btcb::mdb_store::upgrade_v4_to_v5 (btcb::transaction const & transaction_a)
{
	version_put (transaction_a, 5);
	for (auto i (btcb::store_iterator<btcb::account, btcb::account_info_v5> (std::make_unique<btcb::mdb_iterator<btcb::account, btcb::account_info_v5>> (transaction_a, accounts_v0))), n (btcb::store_iterator<btcb::account, btcb::account_info_v5> (nullptr)); i != n; ++i)
	{
		btcb::account_info_v5 info (i->second);
		btcb::block_hash successor (0);
		auto block (block_get (transaction_a, info.head));
		while (block != nullptr)
		{
			auto hash (block->hash ());
			if (block_successor (transaction_a, hash).is_zero () && !successor.is_zero ())
			{
				block_put (transaction_a, hash, *block, successor);
			}
			successor = hash;
			block = block_get (transaction_a, block->previous ());
		}
	}
}

void btcb::mdb_store::upgrade_v5_to_v6 (btcb::transaction const & transaction_a)
{
	version_put (transaction_a, 6);
	std::deque<std::pair<btcb::account, btcb::account_info>> headers;
	for (auto i (btcb::store_iterator<btcb::account, btcb::account_info_v5> (std::make_unique<btcb::mdb_iterator<btcb::account, btcb::account_info_v5>> (transaction_a, accounts_v0))), n (btcb::store_iterator<btcb::account, btcb::account_info_v5> (nullptr)); i != n; ++i)
	{
		btcb::account account (i->first);
		btcb::account_info_v5 info_old (i->second);
		uint64_t block_count (0);
		auto hash (info_old.head);
		while (!hash.is_zero ())
		{
			++block_count;
			auto block (block_get (transaction_a, hash));
			assert (block != nullptr);
			hash = block->previous ();
		}
		btcb::account_info info (info_old.head, info_old.rep_block, info_old.open_block, info_old.balance, info_old.modified, block_count, btcb::epoch::epoch_0);
		headers.push_back (std::make_pair (account, info));
	}
	for (auto i (headers.begin ()), n (headers.end ()); i != n; ++i)
	{
		account_put (transaction_a, i->first, i->second);
	}
}

void btcb::mdb_store::upgrade_v6_to_v7 (btcb::transaction const & transaction_a)
{
	version_put (transaction_a, 7);
	mdb_drop (env.tx (transaction_a), unchecked, 0);
}

void btcb::mdb_store::upgrade_v7_to_v8 (btcb::transaction const & transaction_a)
{
	version_put (transaction_a, 8);
	mdb_drop (env.tx (transaction_a), unchecked, 1);
	mdb_dbi_open (env.tx (transaction_a), "unchecked", MDB_CREATE | MDB_DUPSORT, &unchecked);
}

void btcb::mdb_store::upgrade_v8_to_v9 (btcb::transaction const & transaction_a)
{
	version_put (transaction_a, 9);
	MDB_dbi sequence;
	mdb_dbi_open (env.tx (transaction_a), "sequence", MDB_CREATE | MDB_DUPSORT, &sequence);
	btcb::genesis genesis;
	std::shared_ptr<btcb::block> block (std::move (genesis.open));
	btcb::keypair junk;
	for (btcb::mdb_iterator<btcb::account, uint64_t> i (transaction_a, sequence), n (btcb::mdb_iterator<btcb::account, uint64_t> (nullptr)); i != n; ++i)
	{
		btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (i->second.data ()), i->second.size ());
		uint64_t sequence;
		auto error (btcb::read (stream, sequence));
		// Create a dummy vote with the same sequence number for easy upgrading.  This won't have a valid signature.
		btcb::vote dummy (btcb::account (i->first), junk.prv, sequence, block);
		std::vector<uint8_t> vector;
		{
			btcb::vectorstream stream (vector);
			dummy.serialize (stream);
		}
		auto status1 (mdb_put (env.tx (transaction_a), vote, btcb::mdb_val (i->first), btcb::mdb_val (vector.size (), vector.data ()), 0));
		release_assert (status1 == 0);
		assert (!error);
	}
	mdb_drop (env.tx (transaction_a), sequence, 1);
}

void btcb::mdb_store::upgrade_v9_to_v10 (btcb::transaction const & transaction_a)
{
	//std::cerr << boost::str (boost::format ("Performing database upgrade to version 10...\n"));
	version_put (transaction_a, 10);
	for (auto i (latest_v0_begin (transaction_a)), n (latest_v0_end ()); i != n; ++i)
	{
		btcb::account_info info (i->second);
		if (info.block_count >= block_info_max)
		{
			btcb::account account (i->first);
			//std::cerr << boost::str (boost::format ("Upgrading account %1%...\n") % account.to_account ());
			size_t block_count (1);
			auto hash (info.open_block);
			while (!hash.is_zero ())
			{
				if ((block_count % block_info_max) == 0)
				{
					btcb::block_info block_info;
					block_info.account = account;
					btcb::amount balance (block_balance (transaction_a, hash));
					block_info.balance = balance;
					block_info_put (transaction_a, hash, block_info);
				}
				hash = block_successor (transaction_a, hash);
				++block_count;
			}
		}
	}
}

void btcb::mdb_store::upgrade_v10_to_v11 (btcb::transaction const & transaction_a)
{
	version_put (transaction_a, 11);
	MDB_dbi unsynced;
	mdb_dbi_open (env.tx (transaction_a), "unsynced", MDB_CREATE | MDB_DUPSORT, &unsynced);
	mdb_drop (env.tx (transaction_a), unsynced, 1);
}

void btcb::mdb_store::upgrade_v11_to_v12 (btcb::transaction const & transaction_a)
{
	version_put (transaction_a, 12);
	mdb_drop (env.tx (transaction_a), unchecked, 1);
	mdb_dbi_open (env.tx (transaction_a), "unchecked", MDB_CREATE, &unchecked);
}

void btcb::mdb_store::clear (MDB_dbi db_a)
{
	auto transaction (tx_begin_write ());
	auto status (mdb_drop (env.tx (transaction), db_a, 0));
	release_assert (status == 0);
}

btcb::uint128_t btcb::mdb_store::block_balance (btcb::transaction const & transaction_a, btcb::block_hash const & hash_a)
{
	summation_visitor visitor (transaction_a, *this);
	return visitor.compute_balance (hash_a);
}

btcb::epoch btcb::mdb_store::block_version (btcb::transaction const & transaction_a, btcb::block_hash const & hash_a)
{
	btcb::mdb_val value;
	auto status (mdb_get (env.tx (transaction_a), state_blocks_v1, btcb::mdb_val (hash_a), value));
	release_assert (status == 0 || status == MDB_NOTFOUND);
	return status == 0 ? btcb::epoch::epoch_1 : btcb::epoch::epoch_0;
}

void btcb::mdb_store::representation_add (btcb::transaction const & transaction_a, btcb::block_hash const & source_a, btcb::uint128_t const & amount_a)
{
	auto source_block (block_get (transaction_a, source_a));
	assert (source_block != nullptr);
	auto source_rep (source_block->representative ());
	auto source_previous (representation_get (transaction_a, source_rep));
	representation_put (transaction_a, source_rep, source_previous + amount_a);
}

MDB_dbi btcb::mdb_store::block_database (btcb::block_type type_a, btcb::epoch epoch_a)
{
	if (type_a == btcb::block_type::state)
	{
		assert (epoch_a == btcb::epoch::epoch_0 || epoch_a == btcb::epoch::epoch_1);
	}
	else
	{
		assert (epoch_a == btcb::epoch::epoch_0);
	}
	MDB_dbi result;
	switch (type_a)
	{
		case btcb::block_type::send:
			result = send_blocks;
			break;
		case btcb::block_type::receive:
			result = receive_blocks;
			break;
		case btcb::block_type::open:
			result = open_blocks;
			break;
		case btcb::block_type::change:
			result = change_blocks;
			break;
		case btcb::block_type::state:
			switch (epoch_a)
			{
				case btcb::epoch::epoch_0:
					result = state_blocks_v0;
					break;
				case btcb::epoch::epoch_1:
					result = state_blocks_v1;
					break;
				default:
					assert (false);
			}
			break;
		default:
			assert (false);
			break;
	}
	return result;
}

void btcb::mdb_store::block_raw_put (btcb::transaction const & transaction_a, MDB_dbi database_a, btcb::block_hash const & hash_a, MDB_val value_a)
{
	auto status2 (mdb_put (env.tx (transaction_a), database_a, btcb::mdb_val (hash_a), &value_a, 0));
	release_assert (status2 == 0);
}

void btcb::mdb_store::block_put (btcb::transaction const & transaction_a, btcb::block_hash const & hash_a, btcb::block const & block_a, btcb::block_hash const & successor_a, btcb::epoch epoch_a)
{
	assert (successor_a.is_zero () || block_exists (transaction_a, successor_a));
	std::vector<uint8_t> vector;
	{
		btcb::vectorstream stream (vector);
		block_a.serialize (stream);
		btcb::write (stream, successor_a.bytes);
	}
	block_raw_put (transaction_a, block_database (block_a.type (), epoch_a), hash_a, { vector.size (), vector.data () });
	btcb::block_predecessor_set predecessor (transaction_a, *this);
	block_a.visit (predecessor);
	assert (block_a.previous ().is_zero () || block_successor (transaction_a, block_a.previous ()) == hash_a);
}

MDB_val btcb::mdb_store::block_raw_get (btcb::transaction const & transaction_a, btcb::block_hash const & hash_a, btcb::block_type & type_a)
{
	btcb::mdb_val result;
	auto status (mdb_get (env.tx (transaction_a), send_blocks, btcb::mdb_val (hash_a), result));
	release_assert (status == 0 || status == MDB_NOTFOUND);
	if (status != 0)
	{
		auto status (mdb_get (env.tx (transaction_a), receive_blocks, btcb::mdb_val (hash_a), result));
		release_assert (status == 0 || status == MDB_NOTFOUND);
		if (status != 0)
		{
			auto status (mdb_get (env.tx (transaction_a), open_blocks, btcb::mdb_val (hash_a), result));
			release_assert (status == 0 || status == MDB_NOTFOUND);
			if (status != 0)
			{
				auto status (mdb_get (env.tx (transaction_a), change_blocks, btcb::mdb_val (hash_a), result));
				release_assert (status == 0 || status == MDB_NOTFOUND);
				if (status != 0)
				{
					auto status (mdb_get (env.tx (transaction_a), state_blocks_v0, btcb::mdb_val (hash_a), result));
					release_assert (status == 0 || status == MDB_NOTFOUND);
					if (status != 0)
					{
						auto status (mdb_get (env.tx (transaction_a), state_blocks_v1, btcb::mdb_val (hash_a), result));
						release_assert (status == 0 || status == MDB_NOTFOUND);
						if (status != 0)
						{
							// Block not found
						}
						else
						{
							type_a = btcb::block_type::state;
						}
					}
					else
					{
						type_a = btcb::block_type::state;
					}
				}
				else
				{
					type_a = btcb::block_type::change;
				}
			}
			else
			{
				type_a = btcb::block_type::open;
			}
		}
		else
		{
			type_a = btcb::block_type::receive;
		}
	}
	else
	{
		type_a = btcb::block_type::send;
	}
	return result;
}

template <typename T>
std::shared_ptr<btcb::block> btcb::mdb_store::block_random (btcb::transaction const & transaction_a, MDB_dbi database)
{
	btcb::block_hash hash;
	btcb::random_pool.GenerateBlock (hash.bytes.data (), hash.bytes.size ());
	btcb::store_iterator<btcb::block_hash, std::shared_ptr<T>> existing (std::make_unique<btcb::mdb_iterator<btcb::block_hash, std::shared_ptr<T>>> (transaction_a, database, btcb::mdb_val (hash)));
	if (existing == btcb::store_iterator<btcb::block_hash, std::shared_ptr<T>> (nullptr))
	{
		existing = btcb::store_iterator<btcb::block_hash, std::shared_ptr<T>> (std::make_unique<btcb::mdb_iterator<btcb::block_hash, std::shared_ptr<T>>> (transaction_a, database));
	}
	auto end (btcb::store_iterator<btcb::block_hash, std::shared_ptr<T>> (nullptr));
	assert (existing != end);
	return block_get (transaction_a, btcb::block_hash (existing->first));
}

std::shared_ptr<btcb::block> btcb::mdb_store::block_random (btcb::transaction const & transaction_a)
{
	auto count (block_count (transaction_a));
	auto region (btcb::random_pool.GenerateWord32 (0, count.sum () - 1));
	std::shared_ptr<btcb::block> result;
	if (region < count.send)
	{
		result = block_random<btcb::send_block> (transaction_a, send_blocks);
	}
	else
	{
		region -= count.send;
		if (region < count.receive)
		{
			result = block_random<btcb::receive_block> (transaction_a, receive_blocks);
		}
		else
		{
			region -= count.receive;
			if (region < count.open)
			{
				result = block_random<btcb::open_block> (transaction_a, open_blocks);
			}
			else
			{
				region -= count.open;
				if (region < count.change)
				{
					result = block_random<btcb::change_block> (transaction_a, change_blocks);
				}
				else
				{
					region -= count.change;
					if (region < count.state_v0)
					{
						result = block_random<btcb::state_block> (transaction_a, state_blocks_v0);
					}
					else
					{
						result = block_random<btcb::state_block> (transaction_a, state_blocks_v1);
					}
				}
			}
		}
	}
	assert (result != nullptr);
	return result;
}

btcb::block_hash btcb::mdb_store::block_successor (btcb::transaction const & transaction_a, btcb::block_hash const & hash_a)
{
	btcb::block_type type;
	auto value (block_raw_get (transaction_a, hash_a, type));
	btcb::block_hash result;
	if (value.mv_size != 0)
	{
		assert (value.mv_size >= result.bytes.size ());
		btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.mv_data) + value.mv_size - result.bytes.size (), result.bytes.size ());
		auto error (btcb::read (stream, result.bytes));
		assert (!error);
	}
	else
	{
		result.clear ();
	}
	return result;
}

void btcb::mdb_store::block_successor_clear (btcb::transaction const & transaction_a, btcb::block_hash const & hash_a)
{
	auto block (block_get (transaction_a, hash_a));
	auto version (block_version (transaction_a, hash_a));
	block_put (transaction_a, hash_a, *block, 0, version);
}

std::shared_ptr<btcb::block> btcb::mdb_store::block_get (btcb::transaction const & transaction_a, btcb::block_hash const & hash_a)
{
	btcb::block_type type;
	auto value (block_raw_get (transaction_a, hash_a, type));
	std::shared_ptr<btcb::block> result;
	if (value.mv_size != 0)
	{
		btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.mv_data), value.mv_size);
		result = btcb::deserialize_block (stream, type);
		assert (result != nullptr);
	}
	return result;
}

void btcb::mdb_store::block_del (btcb::transaction const & transaction_a, btcb::block_hash const & hash_a)
{
	auto status (mdb_del (env.tx (transaction_a), state_blocks_v1, btcb::mdb_val (hash_a), nullptr));
	release_assert (status == 0 || status == MDB_NOTFOUND);
	if (status != 0)
	{
		auto status (mdb_del (env.tx (transaction_a), state_blocks_v0, btcb::mdb_val (hash_a), nullptr));
		release_assert (status == 0 || status == MDB_NOTFOUND);
		if (status != 0)
		{
			auto status (mdb_del (env.tx (transaction_a), send_blocks, btcb::mdb_val (hash_a), nullptr));
			release_assert (status == 0 || status == MDB_NOTFOUND);
			if (status != 0)
			{
				auto status (mdb_del (env.tx (transaction_a), receive_blocks, btcb::mdb_val (hash_a), nullptr));
				release_assert (status == 0 || status == MDB_NOTFOUND);
				if (status != 0)
				{
					auto status (mdb_del (env.tx (transaction_a), open_blocks, btcb::mdb_val (hash_a), nullptr));
					release_assert (status == 0 || status == MDB_NOTFOUND);
					if (status != 0)
					{
						auto status (mdb_del (env.tx (transaction_a), change_blocks, btcb::mdb_val (hash_a), nullptr));
						release_assert (status == 0);
					}
				}
			}
		}
	}
}

bool btcb::mdb_store::block_exists (btcb::transaction const & transaction_a, btcb::block_type type, btcb::block_hash const & hash_a)
{
	auto exists (false);
	btcb::mdb_val junk;

	switch (type)
	{
		case btcb::block_type::send:
		{
			auto status (mdb_get (env.tx (transaction_a), send_blocks, btcb::mdb_val (hash_a), junk));
			assert (status == 0 || status == MDB_NOTFOUND);
			exists = status == 0;
			break;
		}
		case btcb::block_type::receive:
		{
			auto status (mdb_get (env.tx (transaction_a), receive_blocks, btcb::mdb_val (hash_a), junk));
			release_assert (status == 0 || status == MDB_NOTFOUND);
			exists = status == 0;
			break;
		}
		case btcb::block_type::open:
		{
			auto status (mdb_get (env.tx (transaction_a), open_blocks, btcb::mdb_val (hash_a), junk));
			release_assert (status == 0 || status == MDB_NOTFOUND);
			exists = status == 0;
			break;
		}
		case btcb::block_type::change:
		{
			auto status (mdb_get (env.tx (transaction_a), change_blocks, btcb::mdb_val (hash_a), junk));
			release_assert (status == 0 || status == MDB_NOTFOUND);
			exists = status == 0;
			break;
		}
		case btcb::block_type::state:
		{
			auto status (mdb_get (env.tx (transaction_a), state_blocks_v0, btcb::mdb_val (hash_a), junk));
			release_assert (status == 0 || status == MDB_NOTFOUND);
			exists = status == 0;
			if (!exists)
			{
				auto status (mdb_get (env.tx (transaction_a), state_blocks_v1, btcb::mdb_val (hash_a), junk));
				release_assert (status == 0 || status == MDB_NOTFOUND);
				exists = status == 0;
			}
			break;
		}
		case btcb::block_type::invalid:
		case btcb::block_type::not_a_block:
			break;
	}

	return exists;
}

bool btcb::mdb_store::block_exists (btcb::transaction const & tx_a, btcb::block_hash const & hash_a)
{
	// clang-format off
	return
		block_exists (tx_a, btcb::block_type::send, hash_a) ||
		block_exists (tx_a, btcb::block_type::receive, hash_a) ||
		block_exists (tx_a, btcb::block_type::open, hash_a) ||
		block_exists (tx_a, btcb::block_type::change, hash_a) ||
		block_exists (tx_a, btcb::block_type::state, hash_a);
	// clang-format on
}

btcb::block_counts btcb::mdb_store::block_count (btcb::transaction const & transaction_a)
{
	btcb::block_counts result;
	MDB_stat send_stats;
	auto status1 (mdb_stat (env.tx (transaction_a), send_blocks, &send_stats));
	release_assert (status1 == 0);
	MDB_stat receive_stats;
	auto status2 (mdb_stat (env.tx (transaction_a), receive_blocks, &receive_stats));
	release_assert (status2 == 0);
	MDB_stat open_stats;
	auto status3 (mdb_stat (env.tx (transaction_a), open_blocks, &open_stats));
	release_assert (status3 == 0);
	MDB_stat change_stats;
	auto status4 (mdb_stat (env.tx (transaction_a), change_blocks, &change_stats));
	release_assert (status4 == 0);
	MDB_stat state_v0_stats;
	auto status5 (mdb_stat (env.tx (transaction_a), state_blocks_v0, &state_v0_stats));
	release_assert (status5 == 0);
	MDB_stat state_v1_stats;
	auto status6 (mdb_stat (env.tx (transaction_a), state_blocks_v1, &state_v1_stats));
	release_assert (status6 == 0);
	result.send = send_stats.ms_entries;
	result.receive = receive_stats.ms_entries;
	result.open = open_stats.ms_entries;
	result.change = change_stats.ms_entries;
	result.state_v0 = state_v0_stats.ms_entries;
	result.state_v1 = state_v1_stats.ms_entries;
	return result;
}

bool btcb::mdb_store::root_exists (btcb::transaction const & transaction_a, btcb::uint256_union const & root_a)
{
	return block_exists (transaction_a, root_a) || account_exists (transaction_a, root_a);
}

void btcb::mdb_store::account_del (btcb::transaction const & transaction_a, btcb::account const & account_a)
{
	auto status1 (mdb_del (env.tx (transaction_a), accounts_v1, btcb::mdb_val (account_a), nullptr));
	if (status1 != 0)
	{
		release_assert (status1 == MDB_NOTFOUND);
		auto status2 (mdb_del (env.tx (transaction_a), accounts_v0, btcb::mdb_val (account_a), nullptr));
		release_assert (status2 == 0);
	}
}

bool btcb::mdb_store::account_exists (btcb::transaction const & transaction_a, btcb::account const & account_a)
{
	auto iterator (latest_begin (transaction_a, account_a));
	return iterator != latest_end () && btcb::account (iterator->first) == account_a;
}

bool btcb::mdb_store::account_get (btcb::transaction const & transaction_a, btcb::account const & account_a, btcb::account_info & info_a)
{
	btcb::mdb_val value;
	auto status1 (mdb_get (env.tx (transaction_a), accounts_v1, btcb::mdb_val (account_a), value));
	release_assert (status1 == 0 || status1 == MDB_NOTFOUND);
	bool result (false);
	btcb::epoch epoch;
	if (status1 == 0)
	{
		epoch = btcb::epoch::epoch_1;
	}
	else
	{
		auto status2 (mdb_get (env.tx (transaction_a), accounts_v0, btcb::mdb_val (account_a), value));
		release_assert (status2 == 0 || status2 == MDB_NOTFOUND);
		if (status2 == 0)
		{
			epoch = btcb::epoch::epoch_0;
		}
		else
		{
			result = true;
		}
	}
	if (!result)
	{
		btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
		info_a.epoch = epoch;
		info_a.deserialize (stream);
	}
	return result;
}

void btcb::mdb_store::frontier_put (btcb::transaction const & transaction_a, btcb::block_hash const & block_a, btcb::account const & account_a)
{
	auto status (mdb_put (env.tx (transaction_a), frontiers, btcb::mdb_val (block_a), btcb::mdb_val (account_a), 0));
	release_assert (status == 0);
}

btcb::account btcb::mdb_store::frontier_get (btcb::transaction const & transaction_a, btcb::block_hash const & block_a)
{
	btcb::mdb_val value;
	auto status (mdb_get (env.tx (transaction_a), frontiers, btcb::mdb_val (block_a), value));
	release_assert (status == 0 || status == MDB_NOTFOUND);
	btcb::account result (0);
	if (status == 0)
	{
		result = btcb::uint256_union (value);
	}
	return result;
}

void btcb::mdb_store::frontier_del (btcb::transaction const & transaction_a, btcb::block_hash const & block_a)
{
	auto status (mdb_del (env.tx (transaction_a), frontiers, btcb::mdb_val (block_a), nullptr));
	release_assert (status == 0);
}

size_t btcb::mdb_store::account_count (btcb::transaction const & transaction_a)
{
	MDB_stat stats1;
	auto status1 (mdb_stat (env.tx (transaction_a), accounts_v0, &stats1));
	release_assert (status1 == 0);
	MDB_stat stats2;
	auto status2 (mdb_stat (env.tx (transaction_a), accounts_v1, &stats2));
	release_assert (status2 == 0);
	auto result (stats1.ms_entries + stats2.ms_entries);
	return result;
}

void btcb::mdb_store::account_put (btcb::transaction const & transaction_a, btcb::account const & account_a, btcb::account_info const & info_a)
{
	MDB_dbi db;
	switch (info_a.epoch)
	{
		case btcb::epoch::invalid:
		case btcb::epoch::unspecified:
			assert (false);
		case btcb::epoch::epoch_0:
			db = accounts_v0;
			break;
		case btcb::epoch::epoch_1:
			db = accounts_v1;
			break;
	}
	auto status (mdb_put (env.tx (transaction_a), db, btcb::mdb_val (account_a), btcb::mdb_val (info_a), 0));
	release_assert (status == 0);
}

void btcb::mdb_store::pending_put (btcb::transaction const & transaction_a, btcb::pending_key const & key_a, btcb::pending_info const & pending_a)
{
	MDB_dbi db;
	switch (pending_a.epoch)
	{
		case btcb::epoch::invalid:
		case btcb::epoch::unspecified:
			assert (false);
		case btcb::epoch::epoch_0:
			db = pending_v0;
			break;
		case btcb::epoch::epoch_1:
			db = pending_v1;
			break;
	}
	auto status (mdb_put (env.tx (transaction_a), db, btcb::mdb_val (key_a), btcb::mdb_val (pending_a), 0));
	release_assert (status == 0);
}

void btcb::mdb_store::pending_del (btcb::transaction const & transaction_a, btcb::pending_key const & key_a)
{
	auto status1 (mdb_del (env.tx (transaction_a), pending_v1, mdb_val (key_a), nullptr));
	if (status1 != 0)
	{
		release_assert (status1 == MDB_NOTFOUND);
		auto status2 (mdb_del (env.tx (transaction_a), pending_v0, mdb_val (key_a), nullptr));
		release_assert (status2 == 0);
	}
}

bool btcb::mdb_store::pending_exists (btcb::transaction const & transaction_a, btcb::pending_key const & key_a)
{
	auto iterator (pending_begin (transaction_a, key_a));
	return iterator != pending_end () && btcb::pending_key (iterator->first) == key_a;
}

bool btcb::mdb_store::pending_get (btcb::transaction const & transaction_a, btcb::pending_key const & key_a, btcb::pending_info & pending_a)
{
	btcb::mdb_val value;
	auto status1 (mdb_get (env.tx (transaction_a), pending_v1, mdb_val (key_a), value));
	release_assert (status1 == 0 || status1 == MDB_NOTFOUND);
	bool result (false);
	btcb::epoch epoch;
	if (status1 == 0)
	{
		epoch = btcb::epoch::epoch_1;
	}
	else
	{
		auto status2 (mdb_get (env.tx (transaction_a), pending_v0, mdb_val (key_a), value));
		release_assert (status2 == 0 || status2 == MDB_NOTFOUND);
		if (status2 == 0)
		{
			epoch = btcb::epoch::epoch_0;
		}
		else
		{
			result = true;
		}
	}
	if (!result)
	{
		btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
		pending_a.epoch = epoch;
		pending_a.deserialize (stream);
	}
	return result;
}

btcb::store_iterator<btcb::pending_key, btcb::pending_info> btcb::mdb_store::pending_begin (btcb::transaction const & transaction_a, btcb::pending_key const & key_a)
{
	btcb::store_iterator<btcb::pending_key, btcb::pending_info> result (std::make_unique<btcb::mdb_merge_iterator<btcb::pending_key, btcb::pending_info>> (transaction_a, pending_v0, pending_v1, mdb_val (key_a)));
	return result;
}

btcb::store_iterator<btcb::pending_key, btcb::pending_info> btcb::mdb_store::pending_begin (btcb::transaction const & transaction_a)
{
	btcb::store_iterator<btcb::pending_key, btcb::pending_info> result (std::make_unique<btcb::mdb_merge_iterator<btcb::pending_key, btcb::pending_info>> (transaction_a, pending_v0, pending_v1));
	return result;
}

btcb::store_iterator<btcb::pending_key, btcb::pending_info> btcb::mdb_store::pending_end ()
{
	btcb::store_iterator<btcb::pending_key, btcb::pending_info> result (nullptr);
	return result;
}

btcb::store_iterator<btcb::pending_key, btcb::pending_info> btcb::mdb_store::pending_v0_begin (btcb::transaction const & transaction_a, btcb::pending_key const & key_a)
{
	btcb::store_iterator<btcb::pending_key, btcb::pending_info> result (std::make_unique<btcb::mdb_iterator<btcb::pending_key, btcb::pending_info>> (transaction_a, pending_v0, mdb_val (key_a)));
	return result;
}

btcb::store_iterator<btcb::pending_key, btcb::pending_info> btcb::mdb_store::pending_v0_begin (btcb::transaction const & transaction_a)
{
	btcb::store_iterator<btcb::pending_key, btcb::pending_info> result (std::make_unique<btcb::mdb_iterator<btcb::pending_key, btcb::pending_info>> (transaction_a, pending_v0));
	return result;
}

btcb::store_iterator<btcb::pending_key, btcb::pending_info> btcb::mdb_store::pending_v0_end ()
{
	btcb::store_iterator<btcb::pending_key, btcb::pending_info> result (nullptr);
	return result;
}

btcb::store_iterator<btcb::pending_key, btcb::pending_info> btcb::mdb_store::pending_v1_begin (btcb::transaction const & transaction_a, btcb::pending_key const & key_a)
{
	btcb::store_iterator<btcb::pending_key, btcb::pending_info> result (std::make_unique<btcb::mdb_iterator<btcb::pending_key, btcb::pending_info>> (transaction_a, pending_v1, mdb_val (key_a)));
	return result;
}

btcb::store_iterator<btcb::pending_key, btcb::pending_info> btcb::mdb_store::pending_v1_begin (btcb::transaction const & transaction_a)
{
	btcb::store_iterator<btcb::pending_key, btcb::pending_info> result (std::make_unique<btcb::mdb_iterator<btcb::pending_key, btcb::pending_info>> (transaction_a, pending_v1));
	return result;
}

btcb::store_iterator<btcb::pending_key, btcb::pending_info> btcb::mdb_store::pending_v1_end ()
{
	btcb::store_iterator<btcb::pending_key, btcb::pending_info> result (nullptr);
	return result;
}

void btcb::mdb_store::block_info_put (btcb::transaction const & transaction_a, btcb::block_hash const & hash_a, btcb::block_info const & block_info_a)
{
	auto status (mdb_put (env.tx (transaction_a), blocks_info, btcb::mdb_val (hash_a), btcb::mdb_val (block_info_a), 0));
	release_assert (status == 0);
}

void btcb::mdb_store::block_info_del (btcb::transaction const & transaction_a, btcb::block_hash const & hash_a)
{
	auto status (mdb_del (env.tx (transaction_a), blocks_info, btcb::mdb_val (hash_a), nullptr));
	release_assert (status == 0);
}

bool btcb::mdb_store::block_info_exists (btcb::transaction const & transaction_a, btcb::block_hash const & hash_a)
{
	auto iterator (block_info_begin (transaction_a, hash_a));
	return iterator != block_info_end () && btcb::block_hash (iterator->first) == hash_a;
}

bool btcb::mdb_store::block_info_get (btcb::transaction const & transaction_a, btcb::block_hash const & hash_a, btcb::block_info & block_info_a)
{
	btcb::mdb_val value;
	auto status (mdb_get (env.tx (transaction_a), blocks_info, btcb::mdb_val (hash_a), value));
	release_assert (status == 0 || status == MDB_NOTFOUND);
	bool result (true);
	if (status != MDB_NOTFOUND)
	{
		result = false;
		assert (value.size () == sizeof (block_info_a.account.bytes) + sizeof (block_info_a.balance.bytes));
		btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
		auto error1 (btcb::read (stream, block_info_a.account));
		assert (!error1);
		auto error2 (btcb::read (stream, block_info_a.balance));
		assert (!error2);
	}
	return result;
}

btcb::uint128_t btcb::mdb_store::representation_get (btcb::transaction const & transaction_a, btcb::account const & account_a)
{
	btcb::mdb_val value;
	auto status (mdb_get (env.tx (transaction_a), representation, btcb::mdb_val (account_a), value));
	release_assert (status == 0 || status == MDB_NOTFOUND);
	btcb::uint128_t result = 0;
	if (status == 0)
	{
		btcb::uint128_union rep;
		btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
		auto error (btcb::read (stream, rep));
		assert (!error);
		result = rep.number ();
	}
	return result;
}

void btcb::mdb_store::representation_put (btcb::transaction const & transaction_a, btcb::account const & account_a, btcb::uint128_t const & representation_a)
{
	btcb::uint128_union rep (representation_a);
	auto status (mdb_put (env.tx (transaction_a), representation, btcb::mdb_val (account_a), btcb::mdb_val (rep), 0));
	release_assert (status == 0);
}

void btcb::mdb_store::unchecked_clear (btcb::transaction const & transaction_a)
{
	auto status (mdb_drop (env.tx (transaction_a), unchecked, 0));
	release_assert (status == 0);
}

void btcb::mdb_store::unchecked_put (btcb::transaction const & transaction_a, btcb::unchecked_key const & key_a, std::shared_ptr<btcb::block> const & block_a)
{
	mdb_val block (block_a);
	auto status (mdb_put (env.tx (transaction_a), unchecked, btcb::mdb_val (key_a), block, 0));
	release_assert (status == 0);
}

void btcb::mdb_store::unchecked_put (btcb::transaction const & transaction_a, btcb::block_hash const & hash_a, std::shared_ptr<btcb::block> const & block_a)
{
	btcb::unchecked_key key (hash_a, block_a->hash ());
	unchecked_put (transaction_a, key, block_a);
}

std::shared_ptr<btcb::vote> btcb::mdb_store::vote_get (btcb::transaction const & transaction_a, btcb::account const & account_a)
{
	btcb::mdb_val value;
	auto status (mdb_get (env.tx (transaction_a), vote, btcb::mdb_val (account_a), value));
	release_assert (status == 0 || status == MDB_NOTFOUND);
	if (status == 0)
	{
		std::shared_ptr<btcb::vote> result (value);
		assert (result != nullptr);
		return result;
	}
	return nullptr;
}

std::vector<std::shared_ptr<btcb::block>> btcb::mdb_store::unchecked_get (btcb::transaction const & transaction_a, btcb::block_hash const & hash_a)
{
	std::vector<std::shared_ptr<btcb::block>> result;
	for (auto i (unchecked_begin (transaction_a, btcb::unchecked_key (hash_a, 0))), n (unchecked_end ()); i != n && btcb::block_hash (i->first.key ()) == hash_a; ++i)
	{
		std::shared_ptr<btcb::block> block (i->second);
		result.push_back (block);
	}
	return result;
}

bool btcb::mdb_store::unchecked_exists (btcb::transaction const & transaction_a, btcb::unchecked_key const & key_a)
{
	auto iterator (unchecked_begin (transaction_a, key_a));
	return iterator != unchecked_end () && btcb::unchecked_key (iterator->first) == key_a;
}

void btcb::mdb_store::unchecked_del (btcb::transaction const & transaction_a, btcb::unchecked_key const & key_a)
{
	auto status (mdb_del (env.tx (transaction_a), unchecked, btcb::mdb_val (key_a), nullptr));
	release_assert (status == 0 || status == MDB_NOTFOUND);
}

size_t btcb::mdb_store::unchecked_count (btcb::transaction const & transaction_a)
{
	MDB_stat unchecked_stats;
	auto status (mdb_stat (env.tx (transaction_a), unchecked, &unchecked_stats));
	release_assert (status == 0);
	auto result (unchecked_stats.ms_entries);
	return result;
}

void btcb::mdb_store::checksum_put (btcb::transaction const & transaction_a, uint64_t prefix, uint8_t mask, btcb::uint256_union const & hash_a)
{
	assert ((prefix & 0xff) == 0);
	uint64_t key (prefix | mask);
	auto status (mdb_put (env.tx (transaction_a), checksum, btcb::mdb_val (sizeof (key), &key), btcb::mdb_val (hash_a), 0));
	release_assert (status == 0);
}

bool btcb::mdb_store::checksum_get (btcb::transaction const & transaction_a, uint64_t prefix, uint8_t mask, btcb::uint256_union & hash_a)
{
	assert ((prefix & 0xff) == 0);
	uint64_t key (prefix | mask);
	btcb::mdb_val value;
	auto status (mdb_get (env.tx (transaction_a), checksum, btcb::mdb_val (sizeof (key), &key), value));
	release_assert (status == 0 || status == MDB_NOTFOUND);
	bool result (true);
	if (status == 0)
	{
		result = false;
		btcb::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
		auto error (btcb::read (stream, hash_a));
		assert (!error);
	}
	return result;
}

void btcb::mdb_store::checksum_del (btcb::transaction const & transaction_a, uint64_t prefix, uint8_t mask)
{
	assert ((prefix & 0xff) == 0);
	uint64_t key (prefix | mask);
	auto status (mdb_del (env.tx (transaction_a), checksum, btcb::mdb_val (sizeof (key), &key), nullptr));
	release_assert (status == 0);
}

void btcb::mdb_store::flush (btcb::transaction const & transaction_a)
{
	{
		std::lock_guard<std::mutex> lock (cache_mutex);
		vote_cache_l1.swap (vote_cache_l2);
		vote_cache_l1.clear ();
	}
	for (auto i (vote_cache_l2.begin ()), n (vote_cache_l2.end ()); i != n; ++i)
	{
		std::vector<uint8_t> vector;
		{
			btcb::vectorstream stream (vector);
			i->second->serialize (stream);
		}
		auto status1 (mdb_put (env.tx (transaction_a), vote, btcb::mdb_val (i->first), btcb::mdb_val (vector.size (), vector.data ()), 0));
		release_assert (status1 == 0);
	}
}
std::shared_ptr<btcb::vote> btcb::mdb_store::vote_current (btcb::transaction const & transaction_a, btcb::account const & account_a)
{
	assert (!cache_mutex.try_lock ());
	std::shared_ptr<btcb::vote> result;
	auto existing (vote_cache_l1.find (account_a));
	auto have_existing (true);
	if (existing == vote_cache_l1.end ())
	{
		existing = vote_cache_l2.find (account_a);
		if (existing == vote_cache_l2.end ())
		{
			have_existing = false;
		}
	}
	if (have_existing)
	{
		result = existing->second;
	}
	else
	{
		result = vote_get (transaction_a, account_a);
	}
	return result;
}

std::shared_ptr<btcb::vote> btcb::mdb_store::vote_generate (btcb::transaction const & transaction_a, btcb::account const & account_a, btcb::raw_key const & key_a, std::shared_ptr<btcb::block> block_a)
{
	std::lock_guard<std::mutex> lock (cache_mutex);
	auto result (vote_current (transaction_a, account_a));
	uint64_t sequence ((result ? result->sequence : 0) + 1);
	result = std::make_shared<btcb::vote> (account_a, key_a, sequence, block_a);
	vote_cache_l1[account_a] = result;
	return result;
}

std::shared_ptr<btcb::vote> btcb::mdb_store::vote_generate (btcb::transaction const & transaction_a, btcb::account const & account_a, btcb::raw_key const & key_a, std::vector<btcb::block_hash> blocks_a)
{
	std::lock_guard<std::mutex> lock (cache_mutex);
	auto result (vote_current (transaction_a, account_a));
	uint64_t sequence ((result ? result->sequence : 0) + 1);
	result = std::make_shared<btcb::vote> (account_a, key_a, sequence, blocks_a);
	vote_cache_l1[account_a] = result;
	return result;
}

std::shared_ptr<btcb::vote> btcb::mdb_store::vote_max (btcb::transaction const & transaction_a, std::shared_ptr<btcb::vote> vote_a)
{
	std::lock_guard<std::mutex> lock (cache_mutex);
	auto current (vote_current (transaction_a, vote_a->account));
	auto result (vote_a);
	if (current != nullptr && current->sequence > result->sequence)
	{
		result = current;
	}
	vote_cache_l1[vote_a->account] = result;
	return result;
}

btcb::store_iterator<btcb::account, btcb::account_info> btcb::mdb_store::latest_begin (btcb::transaction const & transaction_a, btcb::account const & account_a)
{
	btcb::store_iterator<btcb::account, btcb::account_info> result (std::make_unique<btcb::mdb_merge_iterator<btcb::account, btcb::account_info>> (transaction_a, accounts_v0, accounts_v1, btcb::mdb_val (account_a)));
	return result;
}

btcb::store_iterator<btcb::account, btcb::account_info> btcb::mdb_store::latest_begin (btcb::transaction const & transaction_a)
{
	btcb::store_iterator<btcb::account, btcb::account_info> result (std::make_unique<btcb::mdb_merge_iterator<btcb::account, btcb::account_info>> (transaction_a, accounts_v0, accounts_v1));
	return result;
}

btcb::store_iterator<btcb::account, btcb::account_info> btcb::mdb_store::latest_end ()
{
	btcb::store_iterator<btcb::account, btcb::account_info> result (nullptr);
	return result;
}

btcb::store_iterator<btcb::account, btcb::account_info> btcb::mdb_store::latest_v0_begin (btcb::transaction const & transaction_a, btcb::account const & account_a)
{
	btcb::store_iterator<btcb::account, btcb::account_info> result (std::make_unique<btcb::mdb_iterator<btcb::account, btcb::account_info>> (transaction_a, accounts_v0, btcb::mdb_val (account_a)));
	return result;
}

btcb::store_iterator<btcb::account, btcb::account_info> btcb::mdb_store::latest_v0_begin (btcb::transaction const & transaction_a)
{
	btcb::store_iterator<btcb::account, btcb::account_info> result (std::make_unique<btcb::mdb_iterator<btcb::account, btcb::account_info>> (transaction_a, accounts_v0));
	return result;
}

btcb::store_iterator<btcb::account, btcb::account_info> btcb::mdb_store::latest_v0_end ()
{
	btcb::store_iterator<btcb::account, btcb::account_info> result (nullptr);
	return result;
}

btcb::store_iterator<btcb::account, btcb::account_info> btcb::mdb_store::latest_v1_begin (btcb::transaction const & transaction_a, btcb::account const & account_a)
{
	btcb::store_iterator<btcb::account, btcb::account_info> result (std::make_unique<btcb::mdb_iterator<btcb::account, btcb::account_info>> (transaction_a, accounts_v1, btcb::mdb_val (account_a)));
	return result;
}

btcb::store_iterator<btcb::account, btcb::account_info> btcb::mdb_store::latest_v1_begin (btcb::transaction const & transaction_a)
{
	btcb::store_iterator<btcb::account, btcb::account_info> result (std::make_unique<btcb::mdb_iterator<btcb::account, btcb::account_info>> (transaction_a, accounts_v1));
	return result;
}

btcb::store_iterator<btcb::account, btcb::account_info> btcb::mdb_store::latest_v1_end ()
{
	btcb::store_iterator<btcb::account, btcb::account_info> result (nullptr);
	return result;
}
