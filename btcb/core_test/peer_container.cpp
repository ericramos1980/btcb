#include <gtest/gtest.h>
#include <btcb/node/node.hpp>

TEST (peer_container, empty_peers)
{
	btcb::peer_container peers (btcb::endpoint{});
	auto list (peers.purge_list (std::chrono::steady_clock::now ()));
	ASSERT_EQ (0, list.size ());
}

TEST (peer_container, no_recontact)
{
	btcb::peer_container peers (btcb::endpoint{});
	auto observed_peer (0);
	auto observed_disconnect (false);
	btcb::endpoint endpoint1 (boost::asio::ip::address_v6::loopback (), 10000);
	ASSERT_EQ (0, peers.size ());
	peers.peer_observer = [&observed_peer](btcb::endpoint const &) { ++observed_peer; };
	peers.disconnect_observer = [&observed_disconnect]() { observed_disconnect = true; };
	ASSERT_FALSE (peers.insert (endpoint1, btcb::protocol_version));
	ASSERT_EQ (1, peers.size ());
	ASSERT_TRUE (peers.insert (endpoint1, btcb::protocol_version));
	auto remaining (peers.purge_list (std::chrono::steady_clock::now () + std::chrono::seconds (5)));
	ASSERT_TRUE (remaining.empty ());
	ASSERT_EQ (1, observed_peer);
	ASSERT_TRUE (observed_disconnect);
}

TEST (peer_container, no_self_incoming)
{
	btcb::endpoint self (boost::asio::ip::address_v6::loopback (), 10000);
	btcb::peer_container peers (self);
	peers.insert (self, 0);
	ASSERT_TRUE (peers.peers.empty ());
}

TEST (peer_container, no_self_contacting)
{
	btcb::endpoint self (boost::asio::ip::address_v6::loopback (), 10000);
	btcb::peer_container peers (self);
	peers.insert (self, 0);
	ASSERT_TRUE (peers.peers.empty ());
}

TEST (peer_container, reserved_peers_no_contact)
{
	btcb::peer_container peers (btcb::endpoint{});
	ASSERT_TRUE (peers.insert (btcb::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0x00000001)), 10000), 0));
	ASSERT_TRUE (peers.insert (btcb::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xc0000201)), 10000), 0));
	ASSERT_TRUE (peers.insert (btcb::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xc6336401)), 10000), 0));
	ASSERT_TRUE (peers.insert (btcb::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xcb007101)), 10000), 0));
	ASSERT_TRUE (peers.insert (btcb::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xe9fc0001)), 10000), 0));
	ASSERT_TRUE (peers.insert (btcb::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xf0000001)), 10000), 0));
	ASSERT_TRUE (peers.insert (btcb::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xffffffff)), 10000), 0));
	ASSERT_EQ (0, peers.size ());
}

TEST (peer_container, split)
{
	btcb::peer_container peers (btcb::endpoint{});
	auto now (std::chrono::steady_clock::now ());
	btcb::endpoint endpoint1 (boost::asio::ip::address_v6::any (), 100);
	btcb::endpoint endpoint2 (boost::asio::ip::address_v6::any (), 101);
	peers.peers.insert (btcb::peer_information (endpoint1, now - std::chrono::seconds (1), now));
	peers.peers.insert (btcb::peer_information (endpoint2, now + std::chrono::seconds (1), now));
	ASSERT_EQ (2, peers.peers.size ());
	auto list (peers.purge_list (now));
	ASSERT_EQ (1, peers.peers.size ());
	ASSERT_EQ (1, list.size ());
	ASSERT_EQ (endpoint2, list[0].endpoint);
}

TEST (peer_container, fill_random_clear)
{
	btcb::peer_container peers (btcb::endpoint{});
	std::array<btcb::endpoint, 8> target;
	std::fill (target.begin (), target.end (), btcb::endpoint (boost::asio::ip::address_v6::loopback (), 10000));
	peers.random_fill (target);
	ASSERT_TRUE (std::all_of (target.begin (), target.end (), [](btcb::endpoint const & endpoint_a) { return endpoint_a == btcb::endpoint (boost::asio::ip::address_v6::any (), 0); }));
}

TEST (peer_container, fill_random_full)
{
	btcb::peer_container peers (btcb::endpoint{});
	for (auto i (0); i < 100; ++i)
	{
		peers.insert (btcb::endpoint (boost::asio::ip::address_v6::loopback (), i), 0);
	}
	std::array<btcb::endpoint, 8> target;
	std::fill (target.begin (), target.end (), btcb::endpoint (boost::asio::ip::address_v6::loopback (), 10000));
	peers.random_fill (target);
	ASSERT_TRUE (std::none_of (target.begin (), target.end (), [](btcb::endpoint const & endpoint_a) { return endpoint_a == btcb::endpoint (boost::asio::ip::address_v6::loopback (), 10000); }));
}

TEST (peer_container, fill_random_part)
{
	btcb::peer_container peers (btcb::endpoint{});
	std::array<btcb::endpoint, 8> target;
	auto half (target.size () / 2);
	for (auto i (0); i < half; ++i)
	{
		peers.insert (btcb::endpoint (boost::asio::ip::address_v6::loopback (), i + 1), 0);
	}
	std::fill (target.begin (), target.end (), btcb::endpoint (boost::asio::ip::address_v6::loopback (), 10000));
	peers.random_fill (target);
	ASSERT_TRUE (std::none_of (target.begin (), target.begin () + half, [](btcb::endpoint const & endpoint_a) { return endpoint_a == btcb::endpoint (boost::asio::ip::address_v6::loopback (), 10000); }));
	ASSERT_TRUE (std::none_of (target.begin (), target.begin () + half, [](btcb::endpoint const & endpoint_a) { return endpoint_a == btcb::endpoint (boost::asio::ip::address_v6::loopback (), 0); }));
	ASSERT_TRUE (std::all_of (target.begin () + half, target.end (), [](btcb::endpoint const & endpoint_a) { return endpoint_a == btcb::endpoint (boost::asio::ip::address_v6::any (), 0); }));
}

TEST (peer_container, list_fanout)
{
	btcb::peer_container peers (btcb::endpoint{});
	auto list1 (peers.list_fanout ());
	ASSERT_TRUE (list1.empty ());
	for (auto i (0); i < 1000; ++i)
	{
		ASSERT_FALSE (peers.insert (btcb::endpoint (boost::asio::ip::address_v6::loopback (), 10000 + i), btcb::protocol_version));
	}
	auto list2 (peers.list_fanout ());
	ASSERT_EQ (32, list2.size ());
}

TEST (peer_container, rep_weight)
{
	btcb::peer_container peers (btcb::endpoint{});
	peers.insert (btcb::endpoint (boost::asio::ip::address_v6::loopback (), 24001), 0);
	ASSERT_TRUE (peers.representatives (1).empty ());
	btcb::endpoint endpoint0 (boost::asio::ip::address_v6::loopback (), 24000);
	btcb::endpoint endpoint1 (boost::asio::ip::address_v6::loopback (), 24002);
	btcb::endpoint endpoint2 (boost::asio::ip::address_v6::loopback (), 24003);
	btcb::amount amount (100);
	peers.insert (endpoint2, btcb::protocol_version);
	peers.insert (endpoint0, btcb::protocol_version);
	peers.insert (endpoint1, btcb::protocol_version);
	btcb::keypair keypair;
	peers.rep_response (endpoint0, keypair.pub, amount);
	auto reps (peers.representatives (1));
	ASSERT_EQ (1, reps.size ());
	ASSERT_EQ (100, reps[0].rep_weight.number ());
	ASSERT_EQ (keypair.pub, reps[0].probable_rep_account);
	ASSERT_EQ (endpoint0, reps[0].endpoint);
}

// Test to make sure we don't repeatedly send keepalive messages to nodes that aren't responding
TEST (peer_container, reachout)
{
	btcb::peer_container peers (btcb::endpoint{});
	btcb::endpoint endpoint0 (boost::asio::ip::address_v6::loopback (), 24000);
	// Make sure having been contacted by them already indicates we shouldn't reach out
	peers.insert (endpoint0, btcb::protocol_version);
	ASSERT_TRUE (peers.reachout (endpoint0));
	btcb::endpoint endpoint1 (boost::asio::ip::address_v6::loopback (), 24001);
	ASSERT_FALSE (peers.reachout (endpoint1));
	// Reaching out to them once should signal we shouldn't reach out again.
	ASSERT_TRUE (peers.reachout (endpoint1));
	// Make sure we don't purge new items
	peers.purge_list (std::chrono::steady_clock::now () - std::chrono::seconds (10));
	ASSERT_TRUE (peers.reachout (endpoint1));
	// Make sure we purge old items
	peers.purge_list (std::chrono::steady_clock::now () + std::chrono::seconds (10));
	ASSERT_FALSE (peers.reachout (endpoint1));
}

TEST (peer_container, depeer)
{
	btcb::peer_container peers (btcb::endpoint{});
	btcb::endpoint endpoint0 (boost::asio::ip::address_v6::loopback (), 24000);
	peers.contacted (endpoint0, btcb::protocol_version_min - 1);
	ASSERT_EQ (0, peers.size ());
}
