module;

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/thread/thread.hpp>

export module kvserver.server;

import kvserver.config;
import kvserver.session;

namespace kvserver {

using std::shared_ptr;
using std::string;

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::io_context;
using boost::asio::signal_set;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
namespace this_coro = boost::asio::this_coro;

const int MAXIMUM_MESSAGE_LENGTH = 1024;

export class Server {
private:
	io_context ioContext;
	uint16_t portNum;
	shared_ptr<Config> config;
	signal_set signals;

public:
	explicit Server(uint16_t port, shared_ptr<Config> kvConfig)
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
