module;

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <iostream>
#include <syncstream>

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

const int MAXIMUM_MESSAGE_LENGTH = 4096;

export class Session : public std::enable_shared_from_this<Session> {
private:
	tcp::socket socket;
	streambuf buffer;
	shared_ptr<Config> config;
	shared_ptr<Statistic> statistic;

public:
	explicit Session(
		tcp::socket tcpSocket,
		const shared_ptr<Config> &kvConfig,
		const shared_ptr<Statistic> &kvStatistic
	)
		: socket(std::move(tcpSocket))
		, buffer(streambuf(MAXIMUM_MESSAGE_LENGTH))
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
	auto formatResponse(const std::string &key, ConfigData &configData)
	{
		return std::format(
			"{}={}\nreads={}\nwrites=1", key, configData.value, configData.reads, configData.writes
		);
	}

	auto processRequestedCommand(const string &requestString) -> string
	{
		auto command = parseCommand(requestString);
		string response { "(empty)" };
		if (const auto commandGet { get_if<CommandGet>(&command) }; commandGet) {
			if (auto configData = config->get(commandGet->key)) {
				response = formatResponse(commandGet->key, *configData);
			}
		} else if (const auto commandSet { get_if<CommandSet>(&command) }; commandSet) {
			auto configData = config->set(commandSet->key, commandSet->value);
			response		= formatResponse(commandSet->key, configData);
		} else if (const auto w { get_if<CommandUndefined>(&command) }; w) {
			// unknown command
		}
		return response;
	}

	auto waitForRequest() -> awaitable<void>
	{
		while (true) {
			try {
				co_await async_read_until(socket, buffer, "\n", use_awaitable);

				string request { std::istreambuf_iterator<char>(&buffer),
								 std::istreambuf_iterator<char>() };

				string response = processRequestedCommand(request);

				statistic->recordCommand();

				co_await async_write(
					socket, boost::asio::buffer(response.data(), response.size()), use_awaitable
				);
			} catch (boost::system::system_error &err) {
				if (err.code() == boost::asio::error::eof) {
					// Remote peer closed connection
				} else {
					std::osyncstream(std::cerr)
						<< "Server error: wait for request: " << err.what() << '\n';
				}
				break;
			} catch (std::exception &err) {
				std::osyncstream(std::cerr)
					<< "Server exception: wait for request: " << err.what() << '\n';
				break;
			}
		}
	}
};

} // namespace kvserver
