#pragma once

#include <btcb/lib/interface.h>
#include <btcb/secure/common.hpp>

#include <boost/asio.hpp>

#include <bitset>

#include <xxhash/xxhash.h>

namespace btcb
{
using endpoint = boost::asio::ip::udp::endpoint;
bool parse_port (std::string const &, uint16_t &);
bool parse_address_port (std::string const &, boost::asio::ip::address &, uint16_t &);
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
bool parse_endpoint (std::string const &, btcb::endpoint &);
bool parse_tcp_endpoint (std::string const &, btcb::tcp_endpoint &);
bool reserved_address (btcb::endpoint const &, bool);
}

namespace
{
uint64_t endpoint_hash_raw (btcb::endpoint const & endpoint_a)
{
	assert (endpoint_a.address ().is_v6 ());
	btcb::uint128_union address;
	address.bytes = endpoint_a.address ().to_v6 ().to_bytes ();
	XXH64_state_t hash;
	XXH64_reset (&hash, 0);
	XXH64_update (&hash, address.bytes.data (), address.bytes.size ());
	auto port (endpoint_a.port ());
	XXH64_update (&hash, &port, sizeof (port));
	auto result (XXH64_digest (&hash));
	return result;
}
uint64_t ip_address_hash_raw (boost::asio::ip::address const & ip_a)
{
	assert (ip_a.is_v6 ());
	btcb::uint128_union bytes;
	bytes.bytes = ip_a.to_v6 ().to_bytes ();
	XXH64_state_t hash;
	XXH64_reset (&hash, 0);
	XXH64_update (&hash, bytes.bytes.data (), bytes.bytes.size ());
	auto result (XXH64_digest (&hash));
	return result;
}

template <size_t size>
struct endpoint_hash
{
};
template <>
struct endpoint_hash<8>
{
	size_t operator() (btcb::endpoint const & endpoint_a) const
	{
		return endpoint_hash_raw (endpoint_a);
	}
};
template <>
struct endpoint_hash<4>
{
	size_t operator() (btcb::endpoint const & endpoint_a) const
	{
		uint64_t big (endpoint_hash_raw (endpoint_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
};
}

namespace std
{
template <>
struct hash<::btcb::endpoint>
{
	size_t operator() (::btcb::endpoint const & endpoint_a) const
	{
		endpoint_hash<sizeof (size_t)> ehash;
		return ehash (endpoint_a);
	}
};
template <size_t size>
struct ip_address_hash
{
};
template <>
struct ip_address_hash<8>
{
	size_t operator() (boost::asio::ip::address const & ip_address_a) const
	{
		return ip_address_hash_raw (ip_address_a);
	}
};
template <>
struct ip_address_hash<4>
{
	size_t operator() (boost::asio::ip::address const & ip_address_a) const
	{
		uint64_t big (ip_address_hash_raw (ip_address_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
};
template <>
struct hash<boost::asio::ip::address>
{
	size_t operator() (boost::asio::ip::address const & ip_a) const
	{
		ip_address_hash<sizeof (size_t)> ihash;
		return ihash (ip_a);
	}
};
}
namespace boost
{
template <>
struct hash<::btcb::endpoint>
{
	size_t operator() (::btcb::endpoint const & endpoint_a) const
	{
		std::hash<::btcb::endpoint> hash;
		return hash (endpoint_a);
	}
};
}

namespace btcb
{
/**
 * Message types are serialized to the network and existing values must thus never change as
 * types are added, removed and reordered in the enum.
 */
enum class message_type : uint8_t
{
	invalid = 0x0,
	not_a_type = 0x1,
	keepalive = 0x2,
	publish = 0x3,
	confirm_req = 0x4,
	confirm_ack = 0x5,
	bulk_pull = 0x6,
	bulk_push = 0x7,
	frontier_req = 0x8,
	bulk_pull_blocks = 0x9,
	node_id_handshake = 0x0a,
	bulk_pull_account = 0x0b
};
enum class bulk_pull_blocks_mode : uint8_t
{
	list_blocks,
	checksum_blocks
};
enum class bulk_pull_account_flags : uint8_t
{
	pending_hash_and_amount = 0x0,
	pending_address_only = 0x1,
	pending_hash_amount_and_address = 0x2
};
class message_visitor;
class message_header
{
public:
	message_header (btcb::message_type);
	message_header (bool &, btcb::stream &);
	void serialize (btcb::stream &) const;
	bool deserialize (btcb::stream &);
	btcb::block_type block_type () const;
	void block_type_set (btcb::block_type);
	static std::array<uint8_t, 2> constexpr magic_number = btcb::btcb_network == btcb::btcb_networks::btcb_test_network ? std::array<uint8_t, 2>{ { 'R', 'A' } } : btcb::btcb_network == btcb::btcb_networks::btcb_beta_network ? std::array<uint8_t, 2>{ { 'R', 'B' } } : std::array<uint8_t, 2>{ { 'R', 'C' } };
	uint8_t version_max;
	uint8_t version_using;
	uint8_t version_min;
	btcb::message_type type;
	std::bitset<16> extensions;
	//static size_t constexpr ipv4_only_position = 1;  // Not in use, deprecated, was conflicting
	//static size_t constexpr bootstrap_server_position = 2;  // Not in use, deprecated
	/*
	 * A better approach might be to return the size of the message
	 * payload based on the header
	 */
	static size_t constexpr bulk_pull_count_present_flag = 0;
	bool bulk_pull_is_count_present () const;

	static std::bitset<16> constexpr block_type_mask = std::bitset<16> (0x0f00);
	inline bool valid_magic () const
	{
		return magic_number[0] == 'R' && magic_number[1] >= 'A' && magic_number[1] <= 'C';
	}
	inline bool valid_network () const
	{
		return (magic_number[1] - 'A') == static_cast<int> (btcb::btcb_network);
	}
};
class message
{
public:
	message (btcb::message_type);
	message (btcb::message_header const &);
	virtual ~message () = default;
	virtual void serialize (btcb::stream &) const = 0;
	virtual void visit (btcb::message_visitor &) const = 0;
	virtual inline std::shared_ptr<std::vector<uint8_t>> to_bytes () const
	{
		std::shared_ptr<std::vector<uint8_t>> bytes (new std::vector<uint8_t>);
		btcb::vectorstream stream (*bytes);
		serialize (stream);
		return bytes;
	}
	btcb::message_header header;
};
class work_pool;
class message_parser
{
public:
	enum class parse_status
	{
		success,
		insufficient_work,
		invalid_header,
		invalid_message_type,
		invalid_keepalive_message,
		invalid_publish_message,
		invalid_confirm_req_message,
		invalid_confirm_ack_message,
		invalid_node_id_handshake_message,
		outdated_version,
		invalid_magic,
		invalid_network
	};
	message_parser (btcb::block_uniquer &, btcb::vote_uniquer &, btcb::message_visitor &, btcb::work_pool &);
	void deserialize_buffer (uint8_t const *, size_t);
	void deserialize_keepalive (btcb::stream &, btcb::message_header const &);
	void deserialize_publish (btcb::stream &, btcb::message_header const &);
	void deserialize_confirm_req (btcb::stream &, btcb::message_header const &);
	void deserialize_confirm_ack (btcb::stream &, btcb::message_header const &);
	void deserialize_node_id_handshake (btcb::stream &, btcb::message_header const &);
	bool at_end (btcb::stream &);
	btcb::block_uniquer & block_uniquer;
	btcb::vote_uniquer & vote_uniquer;
	btcb::message_visitor & visitor;
	btcb::work_pool & pool;
	parse_status status;
	std::string status_string ();
	static const size_t max_safe_udp_message_size;
};
class keepalive : public message
{
public:
	keepalive (bool &, btcb::stream &, btcb::message_header const &);
	keepalive ();
	void visit (btcb::message_visitor &) const override;
	bool deserialize (btcb::stream &);
	void serialize (btcb::stream &) const override;
	bool operator== (btcb::keepalive const &) const;
	std::array<btcb::endpoint, 8> peers;
};
class publish : public message
{
public:
	publish (bool &, btcb::stream &, btcb::message_header const &, btcb::block_uniquer * = nullptr);
	publish (std::shared_ptr<btcb::block>);
	void visit (btcb::message_visitor &) const override;
	bool deserialize (btcb::stream &, btcb::block_uniquer * = nullptr);
	void serialize (btcb::stream &) const override;
	bool operator== (btcb::publish const &) const;
	std::shared_ptr<btcb::block> block;
};
class confirm_req : public message
{
public:
	confirm_req (bool &, btcb::stream &, btcb::message_header const &, btcb::block_uniquer * = nullptr);
	confirm_req (std::shared_ptr<btcb::block>);
	bool deserialize (btcb::stream &, btcb::block_uniquer * = nullptr);
	void serialize (btcb::stream &) const override;
	void visit (btcb::message_visitor &) const override;
	bool operator== (btcb::confirm_req const &) const;
	std::shared_ptr<btcb::block> block;
};
class confirm_ack : public message
{
public:
	confirm_ack (bool &, btcb::stream &, btcb::message_header const &, btcb::vote_uniquer * = nullptr);
	confirm_ack (std::shared_ptr<btcb::vote>);
	bool deserialize (btcb::stream &, btcb::vote_uniquer * = nullptr);
	void serialize (btcb::stream &) const override;
	void visit (btcb::message_visitor &) const override;
	bool operator== (btcb::confirm_ack const &) const;
	std::shared_ptr<btcb::vote> vote;
};
class frontier_req : public message
{
public:
	frontier_req ();
	frontier_req (bool &, btcb::stream &, btcb::message_header const &);
	bool deserialize (btcb::stream &);
	void serialize (btcb::stream &) const override;
	void visit (btcb::message_visitor &) const override;
	bool operator== (btcb::frontier_req const &) const;
	btcb::account start;
	uint32_t age;
	uint32_t count;
};
class bulk_pull : public message
{
public:
	typedef uint32_t count_t;
	bulk_pull ();
	bulk_pull (bool &, btcb::stream &, btcb::message_header const &);
	bool deserialize (btcb::stream &);
	void serialize (btcb::stream &) const override;
	void visit (btcb::message_visitor &) const override;
	btcb::uint256_union start;
	btcb::block_hash end;
	count_t count;
	bool is_count_present () const;
	void set_count_present (bool);
	static size_t constexpr count_present_flag = btcb::message_header::bulk_pull_count_present_flag;
	static size_t constexpr extended_parameters_size = 8;
};
class bulk_pull_account : public message
{
public:
	bulk_pull_account ();
	bulk_pull_account (bool &, btcb::stream &, btcb::message_header const &);
	bool deserialize (btcb::stream &);
	void serialize (btcb::stream &) const override;
	void visit (btcb::message_visitor &) const override;
	btcb::uint256_union account;
	btcb::uint128_union minimum_amount;
	bulk_pull_account_flags flags;
};
class bulk_pull_blocks : public message
{
public:
	bulk_pull_blocks ();
	bulk_pull_blocks (bool &, btcb::stream &, btcb::message_header const &);
	bool deserialize (btcb::stream &);
	void serialize (btcb::stream &) const override;
	void visit (btcb::message_visitor &) const override;
	btcb::block_hash min_hash;
	btcb::block_hash max_hash;
	bulk_pull_blocks_mode mode;
	uint32_t max_count;
};
class bulk_push : public message
{
public:
	bulk_push ();
	bulk_push (btcb::message_header const &);
	bool deserialize (btcb::stream &);
	void serialize (btcb::stream &) const override;
	void visit (btcb::message_visitor &) const override;
};
class node_id_handshake : public message
{
public:
	node_id_handshake (bool &, btcb::stream &, btcb::message_header const &);
	node_id_handshake (boost::optional<btcb::block_hash>, boost::optional<std::pair<btcb::account, btcb::signature>>);
	bool deserialize (btcb::stream &);
	void serialize (btcb::stream &) const override;
	void visit (btcb::message_visitor &) const override;
	bool operator== (btcb::node_id_handshake const &) const;
	bool is_query_flag () const;
	void set_query_flag (bool);
	bool is_response_flag () const;
	void set_response_flag (bool);
	boost::optional<btcb::uint256_union> query;
	boost::optional<std::pair<btcb::account, btcb::signature>> response;
	static size_t constexpr query_flag = 0;
	static size_t constexpr response_flag = 1;
};
class message_visitor
{
public:
	virtual void keepalive (btcb::keepalive const &) = 0;
	virtual void publish (btcb::publish const &) = 0;
	virtual void confirm_req (btcb::confirm_req const &) = 0;
	virtual void confirm_ack (btcb::confirm_ack const &) = 0;
	virtual void bulk_pull (btcb::bulk_pull const &) = 0;
	virtual void bulk_pull_account (btcb::bulk_pull_account const &) = 0;
	virtual void bulk_pull_blocks (btcb::bulk_pull_blocks const &) = 0;
	virtual void bulk_push (btcb::bulk_push const &) = 0;
	virtual void frontier_req (btcb::frontier_req const &) = 0;
	virtual void node_id_handshake (btcb::node_id_handshake const &) = 0;
	virtual ~message_visitor ();
};

/**
 * Returns seconds passed since unix epoch (posix time)
 */
inline uint64_t seconds_since_epoch ()
{
	return std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now ().time_since_epoch ()).count ();
}
}
