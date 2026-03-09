module;

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <iostream>

export module kvserver.session;

import kvserver.config;
import kvserver.utilities;
import kvserver.statistic;

namespace kvserver {

using std::shared_ptr;
using std::string;

using boost::asio::async_read_until;
using boost::asio::async_write;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::streambuf;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;

export class Session : public std::enable_shared_from_this<Session> {
private:
	tcp::socket socket;
	streambuf buffer; // TODO (change to string)
	shared_ptr<Config> config;
	shared_ptr<Statistic> statistic; // TODO (replace to observer lambdaFunc)

public:
	explicit Session(
		tcp::socket tcpSocket,
		const shared_ptr<Config> &kvConfig,
		const shared_ptr<Statistic> &kvStatistic
	)
		: socket(std::move(tcpSocket))
		, config(kvConfig)
		, statistic(kvStatistic)
	{
	}
	Session()									  = delete;
	Session(const Session &)					  = delete;
	Session(const Session &&)					  = delete;
	auto operator=(const Session &) -> Session &  = delete;
	auto operator=(const Session &&) -> Session & = delete;
	~Session()
	{
		shutdown();
	}

	void shutdown()
	{
		if (!socket.is_open()) {
			return;
		}

		boost::system::error_code ignore;
		[[maybe_unused]] auto shutdownRes = socket.shutdown(tcp::socket::shutdown_both, ignore);
		[[maybe_unused]] auto closeRes	  = socket.close(ignore);
	}

	void run()
	{
		co_spawn(
			socket.get_executor(),
			[self = shared_from_this()] { return self->waitForRequest(); },
			detached
		);
	}

private:
	auto processRequestedCommand(const string &requestString) -> string
	{
		auto command = parseCommand(requestString);
		string response { "(empty)" };
		if (const auto commandGet { get_if<CommandGet>(&command) }; commandGet) {
			if (auto value = config->get(commandGet->key)) {
				response = *value;
			}
		} else if (const auto commandSet { get_if<CommandSet>(&command) }; commandSet) {
			config->set(commandSet->key, commandSet->value);
		} else if (const auto w { get_if<CommandUndefined>(&command) }; w) {
			// TODO (write to journal)
			// std::cerr << "Server: received undefined command: " << requestString << '\n';
		}
		return response;
	}

	// TODO (replace to lambda)
	auto waitForRequest() -> awaitable<void>
	{
		while (true) {
			try {
				co_await async_read_until(socket, buffer, "\n", use_awaitable);

				string request { std::istreambuf_iterator<char>(&buffer),
								 std::istreambuf_iterator<char>() };

				string response = processRequestedCommand(request);

				statistic->recordCommand();

				// TODO (Delete)
				// std::cout << "Response '" << response << "', length: " << response.size() <<
				// '\n';

				co_await async_write(
					socket, boost::asio::buffer(response.data(), response.size()), use_awaitable
				);
			} catch (boost::system::system_error &err) {
				if (err.code() == boost::asio::error::eof) {
					// TODO (delete)
					// std::cout << "Remote peer closed connection\n";
					break;
				} else {
					std::cerr << "Server: exception wait for request: " << err.what() << '\n';
				}
			} catch (std::exception &err) {
				std::cerr << "Server: exception wait for request: " << err.what() << '\n';
			}
		}
	}
};

} // namespace kvserver
