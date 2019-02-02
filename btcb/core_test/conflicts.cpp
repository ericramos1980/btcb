#include <gtest/gtest.h>
#include <btcb/node/testing.hpp>

TEST (conflicts, start_stop)
{
	btcb::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	btcb::genesis genesis;
	btcb::keypair key1;
	auto send1 (std::make_shared<btcb::send_block> (genesis.hash (), key1.pub, 0, btcb::test_genesis_key.prv, btcb::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*send1);
	ASSERT_EQ (btcb::process_result::progress, node1.process (*send1).code);
	ASSERT_EQ (0, node1.active.roots.size ());
	node1.active.start (send1);
	ASSERT_EQ (1, node1.active.roots.size ());
	auto root1 (send1->root ());
	auto existing1 (node1.active.roots.find (root1));
	ASSERT_NE (node1.active.roots.end (), existing1);
	auto votes1 (existing1->election);
	ASSERT_NE (nullptr, votes1);
	ASSERT_EQ (1, votes1->last_votes.size ());
}

TEST (conflicts, add_existing)
{
	btcb::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	btcb::genesis genesis;
	btcb::keypair key1;
	auto send1 (std::make_shared<btcb::send_block> (genesis.hash (), key1.pub, 0, btcb::test_genesis_key.prv, btcb::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*send1);
	ASSERT_EQ (btcb::process_result::progress, node1.process (*send1).code);
	node1.active.start (send1);
	btcb::keypair key2;
	auto send2 (std::make_shared<btcb::send_block> (genesis.hash (), key2.pub, 0, btcb::test_genesis_key.prv, btcb::test_genesis_key.pub, 0));
	node1.active.start (send2);
	ASSERT_EQ (1, node1.active.roots.size ());
	auto vote1 (std::make_shared<btcb::vote> (key2.pub, key2.prv, 0, send2));
	node1.active.vote (vote1);
	ASSERT_EQ (1, node1.active.roots.size ());
	auto votes1 (node1.active.roots.find (send2->root ())->election);
	ASSERT_NE (nullptr, votes1);
	ASSERT_EQ (2, votes1->last_votes.size ());
	ASSERT_NE (votes1->last_votes.end (), votes1->last_votes.find (key2.pub));
}

TEST (conflicts, add_two)
{
	btcb::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	btcb::genesis genesis;
	btcb::keypair key1;
	auto send1 (std::make_shared<btcb::send_block> (genesis.hash (), key1.pub, 0, btcb::test_genesis_key.prv, btcb::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*send1);
	ASSERT_EQ (btcb::process_result::progress, node1.process (*send1).code);
	node1.active.start (send1);
	btcb::keypair key2;
	auto send2 (std::make_shared<btcb::send_block> (send1->hash (), key2.pub, 0, btcb::test_genesis_key.prv, btcb::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*send2);
	ASSERT_EQ (btcb::process_result::progress, node1.process (*send2).code);
	node1.active.start (send2);
	ASSERT_EQ (2, node1.active.roots.size ());
}

TEST (vote_uniquer, null)
{
	btcb::block_uniquer block_uniquer;
	btcb::vote_uniquer uniquer (block_uniquer);
	ASSERT_EQ (nullptr, uniquer.unique (nullptr));
}

// Show that an identical vote can be uniqued
TEST (vote_uniquer, same_vote)
{
	btcb::block_uniquer block_uniquer;
	btcb::vote_uniquer uniquer (block_uniquer);
	btcb::keypair key;
	auto vote1 (std::make_shared<btcb::vote> (key.pub, key.prv, 0, std::make_shared<btcb::state_block> (0, 0, 0, 0, 0, key.prv, key.pub, 0)));
	auto vote2 (std::make_shared<btcb::vote> (*vote1));
	ASSERT_EQ (vote1, uniquer.unique (vote1));
	ASSERT_EQ (vote1, uniquer.unique (vote2));
}

// Show that a different vote for the same block will have the block uniqued
TEST (vote_uniquer, same_block)
{
	btcb::block_uniquer block_uniquer;
	btcb::vote_uniquer uniquer (block_uniquer);
	btcb::keypair key1;
	btcb::keypair key2;
	auto block1 (std::make_shared<btcb::state_block> (0, 0, 0, 0, 0, key1.prv, key1.pub, 0));
	auto block2 (std::make_shared<btcb::state_block> (*block1));
	auto vote1 (std::make_shared<btcb::vote> (key1.pub, key1.prv, 0, block1));
	auto vote2 (std::make_shared<btcb::vote> (key1.pub, key1.prv, 0, block2));
	ASSERT_EQ (vote1, uniquer.unique (vote1));
	ASSERT_EQ (vote2, uniquer.unique (vote2));
	ASSERT_NE (vote1, vote2);
	ASSERT_EQ (boost::get<std::shared_ptr<btcb::block>> (vote1->blocks[0]), boost::get<std::shared_ptr<btcb::block>> (vote2->blocks[0]));
}

TEST (vote_uniquer, vbh_one)
{
	btcb::block_uniquer block_uniquer;
	btcb::vote_uniquer uniquer (block_uniquer);
	btcb::keypair key;
	auto block (std::make_shared<btcb::state_block> (0, 0, 0, 0, 0, key.prv, key.pub, 0));
	std::vector<btcb::block_hash> hashes;
	hashes.push_back (block->hash ());
	auto vote1 (std::make_shared<btcb::vote> (key.pub, key.prv, 0, hashes));
	auto vote2 (std::make_shared<btcb::vote> (*vote1));
	ASSERT_EQ (vote1, uniquer.unique (vote1));
	ASSERT_EQ (vote1, uniquer.unique (vote2));
}

TEST (vote_uniquer, vbh_two)
{
	btcb::block_uniquer block_uniquer;
	btcb::vote_uniquer uniquer (block_uniquer);
	btcb::keypair key;
	auto block1 (std::make_shared<btcb::state_block> (0, 0, 0, 0, 0, key.prv, key.pub, 0));
	std::vector<btcb::block_hash> hashes1;
	hashes1.push_back (block1->hash ());
	auto block2 (std::make_shared<btcb::state_block> (1, 0, 0, 0, 0, key.prv, key.pub, 0));
	std::vector<btcb::block_hash> hashes2;
	hashes2.push_back (block2->hash ());
	auto vote1 (std::make_shared<btcb::vote> (key.pub, key.prv, 0, hashes1));
	auto vote2 (std::make_shared<btcb::vote> (key.pub, key.prv, 0, hashes2));
	ASSERT_EQ (vote1, uniquer.unique (vote1));
	ASSERT_EQ (vote2, uniquer.unique (vote2));
}

TEST (vote_uniquer, cleanup)
{
	btcb::block_uniquer block_uniquer;
	btcb::vote_uniquer uniquer (block_uniquer);
	btcb::keypair key;
	auto vote1 (std::make_shared<btcb::vote> (key.pub, key.prv, 0, std::make_shared<btcb::state_block> (0, 0, 0, 0, 0, key.prv, key.pub, 0)));
	auto vote2 (std::make_shared<btcb::vote> (key.pub, key.prv, 1, std::make_shared<btcb::state_block> (0, 0, 0, 0, 0, key.prv, key.pub, 0)));
	auto vote3 (uniquer.unique (vote1));
	auto vote4 (uniquer.unique (vote2));
	vote2.reset ();
	vote4.reset ();
	ASSERT_EQ (2, uniquer.size ());
	auto iterations (0);
	while (uniquer.size () == 2)
	{
		auto vote5 (uniquer.unique (vote1));
		ASSERT_LT (iterations++, 200);
	}
}

TEST (conflicts, reprioritize)
{
	btcb::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	btcb::genesis genesis;
	btcb::keypair key1;
	auto send1 (std::make_shared<btcb::send_block> (genesis.hash (), key1.pub, 0, btcb::test_genesis_key.prv, btcb::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*send1);
	uint64_t difficulty1;
	btcb::work_validate (*send1, &difficulty1);
	node1.process_active (send1);
	node1.block_processor.flush ();
	auto existing1 (node1.active.roots.find (send1->root ()));
	ASSERT_NE (node1.active.roots.end (), existing1);
	ASSERT_EQ (difficulty1, existing1->difficulty);
	node1.work_generate_blocking (*send1, difficulty1);
	uint64_t difficulty2;
	btcb::work_validate (*send1, &difficulty2);
	node1.process_active (send1);
	node1.block_processor.flush ();
	auto existing2 (node1.active.roots.find (send1->root ()));
	ASSERT_NE (node1.active.roots.end (), existing2);
	ASSERT_EQ (difficulty2, existing2->difficulty);
}
