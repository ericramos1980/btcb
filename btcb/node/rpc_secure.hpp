#pragma once
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <btcb/node/rpc.hpp>

namespace btcb
{
/**
 * Specialization of btcb::rpc with TLS support
 */
class rpc_secure : public rpc
{
public:
	rpc_secure (boost::asio::io_service & service_a, btcb::node & node_a, btcb::rpc_config const & config_a);

	/** Starts accepting connections */
	void accept () override;

	/** Installs the server certificate, key and DH, and optionally sets up client certificate verification */
	void load_certs (boost::asio::ssl::context & ctx);

	/**
	 * If client certificates are used, this is called to verify them.
	 * @param preverified The TLS preverification status. The callback may revalidate, such as accepting self-signed certs.
	 */
	bool on_verify_certificate (bool preverified, boost::asio::ssl::verify_context & ctx);

	/** The context needs to be shared between sessions to make resumption work */
	boost::asio::ssl::context ssl_context;
};

/**
 * Specialization of btcb::rpc_connection for establishing TLS connections.
 * Handshakes with client certificates are supported.
 */
class rpc_connection_secure : public rpc_connection
{
public:
	rpc_connection_secure (btcb::node &, btcb::rpc_secure &);
	void parse_connection () override;
	void read () override;
	/** The TLS handshake callback */
	void handle_handshake (const boost::system::error_code & error);
	/** The TLS async shutdown callback */
	void on_shutdown (const boost::system::error_code & error);

private:
	boost::asio::ssl::stream<boost::asio::ip::tcp::socket &> stream;
};
}
