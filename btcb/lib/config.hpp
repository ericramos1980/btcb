#pragma once

#include <chrono>
#include <cstddef>

namespace btcb
{
/**
 * Network variants with different genesis blocks and network parameters
 * @warning Enum values are used for comparison; do not change.
 */
enum class btcb_networks
{
	// Low work parameters, publicly known genesis key, test IP ports
	btcb_test_network = 0,
	// Normal work parameters, secret beta genesis key, beta IP ports
	btcb_beta_network = 1,
	// Normal work parameters, secret live key, live IP ports
	btcb_live_network = 2,
};
btcb::btcb_networks const btcb_network = btcb_networks::ACTIVE_NETWORK;
std::chrono::milliseconds const transaction_timeout = std::chrono::milliseconds (1000);
}
