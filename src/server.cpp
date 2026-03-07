module;

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/thread/thread.hpp>
#include <iostream>

export module kvserver.server;

import kvserver.config;
import kvserver.utilities;

namespace kvserver {

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;

using std::string;
namespace this_coro = boost::asio::this_coro;

const int MAXIMUM_MESSAGE_LENGTH = 1024;

class Session : public std::enable_shared_from_this<Session> {
private:
	boost::asio::ip::tcp::socket socket;
	boost::asio::streambuf buffer;
	std::shared_ptr<Config> config;

public:
	explicit Session(boost::asio::ip::tcp::socket tcpSocket, std::shared_ptr<Config> kvConfig)
		: socket(std::move(tcpSocket))
		, config(kvConfig)
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
		[[maybe_unused]] auto shutdownRes
			= socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore);
		[[maybe_unused]] auto closeRes = socket.close(ignore);
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

	auto waitForRequest() -> awaitable<void>
	{
		try {
			while (true) {
				co_await boost::asio::async_read_until(socket, buffer, "\n", use_awaitable);

				string request { std::istreambuf_iterator<char>(&buffer),
								 std::istreambuf_iterator<char>() };

				string response = processRequestedCommand(request);

				// TODO (Delete)
				std::cout << "Response '" << response << "', length: " << response.size() << '\n';

				co_await boost::asio::async_write(
					socket, boost::asio::buffer(response.data(), response.size()), use_awaitable
				);
			}
		} catch (boost::system::system_error &err) {
			if (err.code() == boost::asio::error::eof) {
				std::cout << "Remote peer closed connection\n";
				shutdown();
			} else {
				std::cerr << "Server: exception wait for request: " << err.what() << '\n';
			}
		} catch (std::exception &err) {
			std::cerr << "Server: exception wait for request: " << err.what() << '\n';
		}
	}
};

export class Server {
private:
	boost::asio::io_context ioContext;
	uint16_t portNum;
	std::shared_ptr<Config> config;
	boost::asio::signal_set signals;

public:
	explicit Server(uint16_t port, std::shared_ptr<Config> kvConfig)
		: portNum(port)
		, config(kvConfig)
		, signals(ioContext, SIGINT, SIGTERM)
	{
	}
	Server()									= delete;
	Server(const Server &)						= delete;
	Server(const Server &&)						= delete;
	auto operator=(const Server &) -> Server &	= delete;
	auto operator=(const Server &&) -> Server & = delete;
	~Server()
	{
		signals.clear();
	}

	void start(std::size_t threadsCount = 0)
	{
		co_spawn(ioContext, listener(), detached);

		if (!threadsCount) {
			threadsCount = (std::max)(static_cast<int>(boost::thread::hardware_concurrency()), 1);
		}
		--threadsCount;

		auto &ios = ioContext;
		boost::thread_group threadsGroup;
		for (std::size_t i = 0; i < threadsCount; ++i) {
			threadsGroup.create_thread([&ios]() { ios.run(); });
		}

		signals.async_wait([&](auto, auto) { stop(); });

		ios.run();
		threadsGroup.join_all();
	}

	void stop()
	{
		ioContext.stop();
	}

	template<class T> void pushTask(const T &task)
	{
		boost::asio::post(ioContext, task);
	}

private:
	auto listener() -> awaitable<void>
	{
		auto executor = co_await this_coro::executor;
		tcp::acceptor acceptor(executor, { tcp::v4(), portNum });
		for (;;) {
			std::make_shared<Session>(co_await acceptor.async_accept(use_awaitable), config)->run();
		}
	}
};

} // namespace kvserver
