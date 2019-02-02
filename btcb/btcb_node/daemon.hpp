#include <btcb/node/node.hpp>
#include <btcb/node/rpc.hpp>

namespace btcb_daemon
{
class daemon
{
public:
	void run (boost::filesystem::path const &, btcb::node_flags const & flags);
};
class daemon_config
{
public:
	daemon_config ();
	bool deserialize_json (bool &, boost::property_tree::ptree &);
	void serialize_json (boost::property_tree::ptree &);
	bool upgrade_json (unsigned, boost::property_tree::ptree &);
	bool rpc_enable;
	btcb::rpc_config rpc;
	btcb::node_config node;
	bool opencl_enable;
	btcb::opencl_config opencl;
	static constexpr int json_version = 2;
};
}
