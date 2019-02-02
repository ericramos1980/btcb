
#include <btcb/node/common.hpp>

#include <btcb/lib/work.hpp>
#include <btcb/node/wallet.hpp>

#include <boost/endian/conversion.hpp>

std::array<uint8_t, 2> constexpr btcb::message_header::magic_number;
std::bitset<16> constexpr btcb::message_header::block_type_mask;

btcb::message_header::message_header (btcb::message_type type_a) :
version_max (btcb::protocol_version),
version_using (btcb::protocol_version),
version_min (btcb::protocol_version_min),
type (type_a)
{
}

btcb::message_header::message_header (bool & error_a, btcb::stream & stream_a)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a);
	}
}

void btcb::message_header::serialize (btcb::stream & stream_a) const
{
	btcb::write (stream_a, btcb::message_header::magic_number);
	btcb::write (stream_a, version_max);
	btcb::write (stream_a, version_using);
	btcb::write (stream_a, version_min);
	btcb::write (stream_a, type);
	btcb::write (stream_a, static_cast<uint16_t> (extensions.to_ullong ()));
}

bool btcb::message_header::deserialize (btcb::stream & stream_a)
{
	uint16_t extensions_l;
	std::array<uint8_t, 2> magic_number_l;
	auto result (btcb::read (stream_a, magic_number_l));
	result = result || magic_number_l != magic_number;
	result = result || btcb::read (stream_a, version_max);
	result = result || btcb::read (stream_a, version_using);
	result = result || btcb::read (stream_a, version_min);
	result = result || btcb::read (stream_a, type);
	result = result || btcb::read (stream_a, extensions_l);
	if (!result)
	{
		extensions = extensions_l;
	}
	return result;
}

btcb::message::message (btcb::message_type type_a) :
header (type_a)
{
}

btcb::message::message (btcb::message_header const & header_a) :
header (header_a)
{
}

btcb::block_type btcb::message_header::block_type () const
{
	return static_cast<btcb::block_type> (((extensions & block_type_mask) >> 8).to_ullong ());
}

void btcb::message_header::block_type_set (btcb::block_type type_a)
{
	extensions &= ~block_type_mask;
	extensions |= std::bitset<16> (static_cast<unsigned long long> (type_a) << 8);
}

bool btcb::message_header::bulk_pull_is_count_present () const
{
	auto result (false);
	if (type == btcb::message_type::bulk_pull)
	{
		if (extensions.test (bulk_pull_count_present_flag))
		{
			result = true;
		}
	}

	return result;
}

// MTU - IP header - UDP header
const size_t btcb::message_parser::max_safe_udp_message_size = 508;

std::string btcb::message_parser::status_string ()
{
	switch (status)
	{
		case btcb::message_parser::parse_status::success:
		{
			return "success";
		}
		case btcb::message_parser::parse_status::insufficient_work:
		{
			return "insufficient_work";
		}
		case btcb::message_parser::parse_status::invalid_header:
		{
			return "invalid_header";
		}
		case btcb::message_parser::parse_status::invalid_message_type:
		{
			return "invalid_message_type";
		}
		case btcb::message_parser::parse_status::invalid_keepalive_message:
		{
			return "invalid_keepalive_message";
		}
		case btcb::message_parser::parse_status::invalid_publish_message:
		{
			return "invalid_publish_message";
		}
		case btcb::message_parser::parse_status::invalid_confirm_req_message:
		{
			return "invalid_confirm_req_message";
		}
		case btcb::message_parser::parse_status::invalid_confirm_ack_message:
		{
			return "invalid_confirm_ack_message";
		}
		case btcb::message_parser::parse_status::invalid_node_id_handshake_message:
		{
			return "invalid_node_id_handshake_message";
		}
		case btcb::message_parser::parse_status::outdated_version:
		{
			return "outdated_version";
		}
		case btcb::message_parser::parse_status::invalid_magic:
		{
			return "invalid_magic";
		}
		case btcb::message_parser::parse_status::invalid_network:
		{
			return "invalid_network";
		}
	}

	assert (false);

	return "[unknown parse_status]";
}

btcb::message_parser::message_parser (btcb::block_uniquer & block_uniquer_a, btcb::vote_uniquer & vote_uniquer_a, btcb::message_visitor & visitor_a, btcb::work_pool & pool_a) :
block_uniquer (block_uniquer_a),
vote_uniquer (vote_uniquer_a),
visitor (visitor_a),
pool (pool_a),
status (parse_status::success)
{
}

void btcb::message_parser::deserialize_buffer (uint8_t const * buffer_a, size_t size_a)
{
	status = parse_status::success;
	auto error (false);
	if (size_a <= max_safe_udp_message_size)
	{
		// Guaranteed to be deliverable
		btcb::bufferstream stream (buffer_a, size_a);
		btcb::message_header header (error, stream);
		if (!error)
		{
			if (btcb::btcb_network == btcb::btcb_networks::btcb_beta_network && header.version_using < btcb::protocol_version_reasonable_min)
			{
				status = parse_status::outdated_version;
			}
			else if (!header.valid_magic ())
			{
				status = parse_status::invalid_magic;
			}
			else if (!header.valid_network ())
			{
				status = parse_status::invalid_network;
			}
			else
			{
				switch (header.type)
				{
					case btcb::message_type::keepalive:
					{
						deserialize_keepalive (stream, header);
						break;
					}
					case btcb::message_type::publish:
					{
						deserialize_publish (stream, header);
						break;
					}
					case btcb::message_type::confirm_req:
					{
						deserialize_confirm_req (stream, header);
						break;
					}
					case btcb::message_type::confirm_ack:
					{
						deserialize_confirm_ack (stream, header);
						break;
					}
					case btcb::message_type::node_id_handshake:
					{
						deserialize_node_id_handshake (stream, header);
						break;
					}
					default:
					{
						status = parse_status::invalid_message_type;
						break;
					}
				}
			}
		}
		else
		{
			status = parse_status::invalid_header;
		}
	}
}

void btcb::message_parser::deserialize_keepalive (btcb::stream & stream_a, btcb::message_header const & header_a)
{
	auto error (false);
	btcb::keepalive incoming (error, stream_a, header_a);
	if (!error && at_end (stream_a))
	{
		visitor.keepalive (incoming);
	}
	else
	{
		status = parse_status::invalid_keepalive_message;
	}
}

void btcb::message_parser::deserialize_publish (btcb::stream & stream_a, btcb::message_header const & header_a)
{
	auto error (false);
	btcb::publish incoming (error, stream_a, header_a, &block_uniquer);
	if (!error && at_end (stream_a))
	{
		if (!btcb::work_validate (*incoming.block))
		{
			visitor.publish (incoming);
		}
		else
		{
			status = parse_status::insufficient_work;
		}
	}
	else
	{
		status = parse_status::invalid_publish_message;
	}
}

void btcb::message_parser::deserialize_confirm_req (btcb::stream & stream_a, btcb::message_header const & header_a)
{
	auto error (false);
	btcb::confirm_req incoming (error, stream_a, header_a, &block_uniquer);
	if (!error && at_end (stream_a))
	{
		if (!btcb::work_validate (*incoming.block))
		{
			visitor.confirm_req (incoming);
		}
		else
		{
			status = parse_status::insufficient_work;
		}
	}
	else
	{
		status = parse_status::invalid_confirm_req_message;
	}
}

void btcb::message_parser::deserialize_confirm_ack (btcb::stream & stream_a, btcb::message_header const & header_a)
{
	auto error (false);
	btcb::confirm_ack incoming (error, stream_a, header_a, &vote_uniquer);
	if (!error && at_end (stream_a))
	{
		for (auto & vote_block : incoming.vote->blocks)
		{
			if (!vote_block.which ())
			{
				auto block (boost::get<std::shared_ptr<btcb::block>> (vote_block));
				if (btcb::work_validate (*block))
				{
					status = parse_status::insufficient_work;
					break;
				}
			}
		}
		if (status == parse_status::success)
		{
			visitor.confirm_ack (incoming);
		}
	}
	else
	{
		status = parse_status::invalid_confirm_ack_message;
	}
}

void btcb::message_parser::deserialize_node_id_handshake (btcb::stream & stream_a, btcb::message_header const & header_a)
{
	bool error_l (false);
	btcb::node_id_handshake incoming (error_l, stream_a, header_a);
	if (!error_l && at_end (stream_a))
	{
		visitor.node_id_handshake (incoming);
	}
	else
	{
		status = parse_status::invalid_node_id_handshake_message;
	}
}

bool btcb::message_parser::at_end (btcb::stream & stream_a)
{
	uint8_t junk;
	auto end (btcb::read (stream_a, junk));
	return end;
}

btcb::keepalive::keepalive () :
message (btcb::message_type::keepalive)
{
	btcb::endpoint endpoint (boost::asio::ip::address_v6{}, 0);
	for (auto i (peers.begin ()), n (peers.end ()); i != n; ++i)
	{
		*i = endpoint;
	}
}

btcb::keepalive::keepalive (bool & error_a, btcb::stream & stream_a, btcb::message_header const & header_a) :
message (header_a)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a);
	}
}

void btcb::keepalive::visit (btcb::message_visitor & visitor_a) const
{
	visitor_a.keepalive (*this);
}

void btcb::keepalive::serialize (btcb::stream & stream_a) const
{
	header.serialize (stream_a);
	for (auto i (peers.begin ()), j (peers.end ()); i != j; ++i)
	{
		assert (i->address ().is_v6 ());
		auto bytes (i->address ().to_v6 ().to_bytes ());
		write (stream_a, bytes);
		write (stream_a, i->port ());
	}
}

bool btcb::keepalive::deserialize (btcb::stream & stream_a)
{
	assert (header.type == btcb::message_type::keepalive);
	auto error (false);
	for (auto i (peers.begin ()), j (peers.end ()); i != j && !error; ++i)
	{
		std::array<uint8_t, 16> address;
		uint16_t port;
		if (!read (stream_a, address) && !read (stream_a, port))
		{
			*i = btcb::endpoint (boost::asio::ip::address_v6 (address), port);
		}
		else
		{
			error = true;
		}
	}
	return error;
}

bool btcb::keepalive::operator== (btcb::keepalive const & other_a) const
{
	return peers == other_a.peers;
}

btcb::publish::publish (bool & error_a, btcb::stream & stream_a, btcb::message_header const & header_a, btcb::block_uniquer * uniquer_a) :
message (header_a)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a, uniquer_a);
	}
}

btcb::publish::publish (std::shared_ptr<btcb::block> block_a) :
message (btcb::message_type::publish),
block (block_a)
{
	header.block_type_set (block->type ());
}

bool btcb::publish::deserialize (btcb::stream & stream_a, btcb::block_uniquer * uniquer_a)
{
	assert (header.type == btcb::message_type::publish);
	block = btcb::deserialize_block (stream_a, header.block_type (), uniquer_a);
	auto result (block == nullptr);
	return result;
}

void btcb::publish::serialize (btcb::stream & stream_a) const
{
	assert (block != nullptr);
	header.serialize (stream_a);
	block->serialize (stream_a);
}

void btcb::publish::visit (btcb::message_visitor & visitor_a) const
{
	visitor_a.publish (*this);
}

bool btcb::publish::operator== (btcb::publish const & other_a) const
{
	return *block == *other_a.block;
}

btcb::confirm_req::confirm_req (bool & error_a, btcb::stream & stream_a, btcb::message_header const & header_a, btcb::block_uniquer * uniquer_a) :
message (header_a)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a, uniquer_a);
	}
}

btcb::confirm_req::confirm_req (std::shared_ptr<btcb::block> block_a) :
message (btcb::message_type::confirm_req),
block (block_a)
{
	header.block_type_set (block->type ());
}

bool btcb::confirm_req::deserialize (btcb::stream & stream_a, btcb::block_uniquer * uniquer_a)
{
	assert (header.type == btcb::message_type::confirm_req);
	block = btcb::deserialize_block (stream_a, header.block_type (), uniquer_a);
	auto result (block == nullptr);
	return result;
}

void btcb::confirm_req::visit (btcb::message_visitor & visitor_a) const
{
	visitor_a.confirm_req (*this);
}

void btcb::confirm_req::serialize (btcb::stream & stream_a) const
{
	assert (block != nullptr);
	header.serialize (stream_a);
	block->serialize (stream_a);
}

bool btcb::confirm_req::operator== (btcb::confirm_req const & other_a) const
{
	return *block == *other_a.block;
}

btcb::confirm_ack::confirm_ack (bool & error_a, btcb::stream & stream_a, btcb::message_header const & header_a, btcb::vote_uniquer * uniquer_a) :
message (header_a),
vote (std::make_shared<btcb::vote> (error_a, stream_a, header.block_type ()))
{
	if (uniquer_a)
	{
		vote = uniquer_a->unique (vote);
	}
}

btcb::confirm_ack::confirm_ack (std::shared_ptr<btcb::vote> vote_a) :
message (btcb::message_type::confirm_ack),
vote (vote_a)
{
	auto & first_vote_block (vote_a->blocks[0]);
	if (first_vote_block.which ())
	{
		header.block_type_set (btcb::block_type::not_a_block);
	}
	else
	{
		header.block_type_set (boost::get<std::shared_ptr<btcb::block>> (first_vote_block)->type ());
	}
}

bool btcb::confirm_ack::deserialize (btcb::stream & stream_a, btcb::vote_uniquer * uniquer_a)
{
	assert (header.type == btcb::message_type::confirm_ack);
	auto result (vote->deserialize (stream_a));
	if (uniquer_a)
	{
		vote = uniquer_a->unique (vote);
	}
	return result;
}

void btcb::confirm_ack::serialize (btcb::stream & stream_a) const
{
	assert (header.block_type () == btcb::block_type::not_a_block || header.block_type () == btcb::block_type::send || header.block_type () == btcb::block_type::receive || header.block_type () == btcb::block_type::open || header.block_type () == btcb::block_type::change || header.block_type () == btcb::block_type::state);
	header.serialize (stream_a);
	vote->serialize (stream_a, header.block_type ());
}

bool btcb::confirm_ack::operator== (btcb::confirm_ack const & other_a) const
{
	auto result (*vote == *other_a.vote);
	return result;
}

void btcb::confirm_ack::visit (btcb::message_visitor & visitor_a) const
{
	visitor_a.confirm_ack (*this);
}

btcb::frontier_req::frontier_req () :
message (btcb::message_type::frontier_req)
{
}

btcb::frontier_req::frontier_req (bool & error_a, btcb::stream & stream_a, btcb::message_header const & header_a) :
message (header_a)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a);
	}
}

bool btcb::frontier_req::deserialize (btcb::stream & stream_a)
{
	assert (header.type == btcb::message_type::frontier_req);
	auto result (read (stream_a, start.bytes));
	if (!result)
	{
		result = read (stream_a, age);
		if (!result)
		{
			result = read (stream_a, count);
		}
	}
	return result;
}

void btcb::frontier_req::serialize (btcb::stream & stream_a) const
{
	header.serialize (stream_a);
	write (stream_a, start.bytes);
	write (stream_a, age);
	write (stream_a, count);
}

void btcb::frontier_req::visit (btcb::message_visitor & visitor_a) const
{
	visitor_a.frontier_req (*this);
}

bool btcb::frontier_req::operator== (btcb::frontier_req const & other_a) const
{
	return start == other_a.start && age == other_a.age && count == other_a.count;
}

btcb::bulk_pull::bulk_pull () :
message (btcb::message_type::bulk_pull),
count (0)
{
}

btcb::bulk_pull::bulk_pull (bool & error_a, btcb::stream & stream_a, btcb::message_header const & header_a) :
message (header_a),
count (0)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a);
	}
}

void btcb::bulk_pull::visit (btcb::message_visitor & visitor_a) const
{
	visitor_a.bulk_pull (*this);
}

bool btcb::bulk_pull::deserialize (btcb::stream & stream_a)
{
	assert (header.type == btcb::message_type::bulk_pull);
	auto result (read (stream_a, start));
	if (!result)
	{
		result = read (stream_a, end);

		if (!result)
		{
			if (is_count_present ())
			{
				std::array<uint8_t, extended_parameters_size> count_buffer;
				static_assert (sizeof (count) < (count_buffer.size () - 1), "count must fit within buffer");

				result = read (stream_a, count_buffer);
				if (count_buffer[0] != 0)
				{
					result = true;
				}
				else
				{
					memcpy (&count, count_buffer.data () + 1, sizeof (count));
					boost::endian::little_to_native_inplace (count);
				}
			}
			else
			{
				count = 0;
			}
		}
	}
	return result;
}

void btcb::bulk_pull::serialize (btcb::stream & stream_a) const
{
	/*
	 * Ensure the "count_present" flag is set if there
	 * is a limit specifed.  Additionally, do not allow
	 * the "count_present" flag with a value of 0, since
	 * that is a sentinel which we use to mean "all blocks"
	 * and that is the behavior of not having the flag set
	 * so it is wasteful to do this.
	 */
	assert ((count == 0 && !is_count_present ()) || (count != 0 && is_count_present ()));

	header.serialize (stream_a);
	write (stream_a, start);
	write (stream_a, end);

	if (is_count_present ())
	{
		std::array<uint8_t, extended_parameters_size> count_buffer{ { 0 } };
		decltype (count) count_little_endian;
		static_assert (sizeof (count_little_endian) < (count_buffer.size () - 1), "count must fit within buffer");

		count_little_endian = boost::endian::native_to_little (count);
		memcpy (count_buffer.data () + 1, &count_little_endian, sizeof (count_little_endian));

		write (stream_a, count_buffer);
	}
}

bool btcb::bulk_pull::is_count_present () const
{
	return header.extensions.test (count_present_flag);
}

void btcb::bulk_pull::set_count_present (bool value_a)
{
	header.extensions.set (count_present_flag, value_a);
}

btcb::bulk_pull_account::bulk_pull_account () :
message (btcb::message_type::bulk_pull_account)
{
}

btcb::bulk_pull_account::bulk_pull_account (bool & error_a, btcb::stream & stream_a, btcb::message_header const & header_a) :
message (header_a)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a);
	}
}

void btcb::bulk_pull_account::visit (btcb::message_visitor & visitor_a) const
{
	visitor_a.bulk_pull_account (*this);
}

bool btcb::bulk_pull_account::deserialize (btcb::stream & stream_a)
{
	assert (header.type == btcb::message_type::bulk_pull_account);
	auto result (read (stream_a, account));
	if (!result)
	{
		result = read (stream_a, minimum_amount);
		if (!result)
		{
			result = read (stream_a, flags);
		}
	}
	return result;
}

void btcb::bulk_pull_account::serialize (btcb::stream & stream_a) const
{
	header.serialize (stream_a);
	write (stream_a, account);
	write (stream_a, minimum_amount);
	write (stream_a, flags);
}

btcb::bulk_pull_blocks::bulk_pull_blocks () :
message (btcb::message_type::bulk_pull_blocks)
{
}

btcb::bulk_pull_blocks::bulk_pull_blocks (bool & error_a, btcb::stream & stream_a, btcb::message_header const & header_a) :
message (header_a)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a);
	}
}

void btcb::bulk_pull_blocks::visit (btcb::message_visitor & visitor_a) const
{
	visitor_a.bulk_pull_blocks (*this);
}

bool btcb::bulk_pull_blocks::deserialize (btcb::stream & stream_a)
{
	assert (header.type == btcb::message_type::bulk_pull_blocks);
	auto result (read (stream_a, min_hash));
	if (!result)
	{
		result = read (stream_a, max_hash);
		if (!result)
		{
			result = read (stream_a, mode);
			if (!result)
			{
				result = read (stream_a, max_count);
			}
		}
	}
	return result;
}

void btcb::bulk_pull_blocks::serialize (btcb::stream & stream_a) const
{
	header.serialize (stream_a);
	write (stream_a, min_hash);
	write (stream_a, max_hash);
	write (stream_a, mode);
	write (stream_a, max_count);
}

btcb::bulk_push::bulk_push () :
message (btcb::message_type::bulk_push)
{
}

btcb::bulk_push::bulk_push (btcb::message_header const & header_a) :
message (header_a)
{
}

bool btcb::bulk_push::deserialize (btcb::stream & stream_a)
{
	assert (header.type == btcb::message_type::bulk_push);
	return false;
}

void btcb::bulk_push::serialize (btcb::stream & stream_a) const
{
	header.serialize (stream_a);
}

void btcb::bulk_push::visit (btcb::message_visitor & visitor_a) const
{
	visitor_a.bulk_push (*this);
}

size_t constexpr btcb::node_id_handshake::query_flag;
size_t constexpr btcb::node_id_handshake::response_flag;

btcb::node_id_handshake::node_id_handshake (bool & error_a, btcb::stream & stream_a, btcb::message_header const & header_a) :
message (header_a),
query (boost::none),
response (boost::none)
{
	error_a = deserialize (stream_a);
}

btcb::node_id_handshake::node_id_handshake (boost::optional<btcb::uint256_union> query, boost::optional<std::pair<btcb::account, btcb::signature>> response) :
message (btcb::message_type::node_id_handshake),
query (query),
response (response)
{
	if (query)
	{
		set_query_flag (true);
	}
	if (response)
	{
		set_response_flag (true);
	}
}

bool btcb::node_id_handshake::deserialize (btcb::stream & stream_a)
{
	auto result (false);
	assert (header.type == btcb::message_type::node_id_handshake);
	if (!result && is_query_flag ())
	{
		btcb::uint256_union query_hash;
		result = read (stream_a, query_hash);
		if (!result)
		{
			query = query_hash;
		}
	}
	if (!result && is_response_flag ())
	{
		btcb::account response_account;
		result = read (stream_a, response_account);
		if (!result)
		{
			btcb::signature response_signature;
			result = read (stream_a, response_signature);
			if (!result)
			{
				response = std::make_pair (response_account, response_signature);
			}
		}
	}
	return result;
}

void btcb::node_id_handshake::serialize (btcb::stream & stream_a) const
{
	header.serialize (stream_a);
	if (query)
	{
		write (stream_a, *query);
	}
	if (response)
	{
		write (stream_a, response->first);
		write (stream_a, response->second);
	}
}

bool btcb::node_id_handshake::operator== (btcb::node_id_handshake const & other_a) const
{
	auto result (*query == *other_a.query && *response == *other_a.response);
	return result;
}

bool btcb::node_id_handshake::is_query_flag () const
{
	return header.extensions.test (query_flag);
}

void btcb::node_id_handshake::set_query_flag (bool value_a)
{
	header.extensions.set (query_flag, value_a);
}

bool btcb::node_id_handshake::is_response_flag () const
{
	return header.extensions.test (response_flag);
}

void btcb::node_id_handshake::set_response_flag (bool value_a)
{
	header.extensions.set (response_flag, value_a);
}

void btcb::node_id_handshake::visit (btcb::message_visitor & visitor_a) const
{
	visitor_a.node_id_handshake (*this);
}

btcb::message_visitor::~message_visitor ()
{
}

bool btcb::parse_port (std::string const & string_a, uint16_t & port_a)
{
	bool result;
	size_t converted;
	try
	{
		port_a = std::stoul (string_a, &converted);
		result = converted != string_a.size () || converted > std::numeric_limits<uint16_t>::max ();
	}
	catch (...)
	{
		result = true;
	}
	return result;
}

bool btcb::parse_address_port (std::string const & string, boost::asio::ip::address & address_a, uint16_t & port_a)
{
	auto result (false);
	auto port_position (string.rfind (':'));
	if (port_position != std::string::npos && port_position > 0)
	{
		std::string port_string (string.substr (port_position + 1));
		try
		{
			uint16_t port;
			result = parse_port (port_string, port);
			if (!result)
			{
				boost::system::error_code ec;
				auto address (boost::asio::ip::address_v6::from_string (string.substr (0, port_position), ec));
				if (!ec)
				{
					address_a = address;
					port_a = port;
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
		}
		catch (...)
		{
			result = true;
		}
	}
	else
	{
		result = true;
	}
	return result;
}

bool btcb::parse_endpoint (std::string const & string, btcb::endpoint & endpoint_a)
{
	boost::asio::ip::address address;
	uint16_t port;
	auto result (parse_address_port (string, address, port));
	if (!result)
	{
		endpoint_a = btcb::endpoint (address, port);
	}
	return result;
}

bool btcb::parse_tcp_endpoint (std::string const & string, btcb::tcp_endpoint & endpoint_a)
{
	boost::asio::ip::address address;
	uint16_t port;
	auto result (parse_address_port (string, address, port));
	if (!result)
	{
		endpoint_a = btcb::tcp_endpoint (address, port);
	}
	return result;
}
