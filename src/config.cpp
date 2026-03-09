module;

#include <boost/json.hpp>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>

export module kvserver.config;

namespace kvserver {

export struct ConfigKeyStats {
	int reads { 0 };
	int writes { 0 };
};

export class Config {
private:
	std::string configFilePath;
	std::unordered_map<std::string, std::string> configData;
	// TODO (move to different statistics class)
	std::unordered_map<std::string, ConfigKeyStats> keyStatus;

	// TODO (Replace to different locks: shared_timed_mutex, std::scoped_lock)
	mutable std::mutex dataMutex;
	mutable std::mutex fileMutex;
	mutable std::mutex statusMutex;

	const std::chrono::seconds saveConfigTimeout { std::chrono::seconds(5) };
	bool isNeedSave { false };
	std::mutex saveMutex;

	std::jthread saveThread;
	std::condition_variable_any saveCV;
	std::queue<std::function<void()>> saveQueue;

public:
	explicit Config(std::string configFilePath)
		: configFilePath(std::move(configFilePath))
		, configData {}
		, keyStatus {}
	{
		loadConfig();
		startSaveThread();
	}

	~Config()
	{
		stopSaveThread();
		if (isNeedSave) {
			saveConfig();
		}
	}

	Config(const Config &)					   = delete;
	auto operator=(const Config &) -> Config & = delete;
	Config(Config &&)						   = delete;
	auto operator=(Config &&) -> Config &	   = delete;

	auto get(const std::string &key) -> std::optional<std::string>
	{
		std::scoped_lock sl(dataMutex, statusMutex);
		auto it = configData.find(key);
		if (it != configData.end()) {
			keyStatus[key].reads++;
			return it->second;
		}

		return {};
	}

	void set(const std::string &key, const std::string &value)
	{
		{
			std::scoped_lock sl(dataMutex, statusMutex);
			configData[key] = value;
			keyStatus[key].writes++;
			isNeedSave = true;
		}

		saveCV.notify_one();
	}

	auto getKeyStats(const std::string &key) -> ConfigKeyStats const
	{
		std::lock_guard<std::mutex> lock(statusMutex);

		auto it = keyStatus.find(key);
		if (it != keyStatus.end()) {
			return it->second;
		}

		return { .reads = 0, .writes = 0 };
	}

private:
	void loadConfig()
	{
		if (!std::filesystem::exists(configFilePath)) {
			std::cerr << "Config file not found, creating new one: " << configFilePath << '\n';

			saveConfig();
			return;
		}

		try {
			std::lock_guard<std::mutex> lock(dataMutex);

			std::ifstream file(configFilePath);
			std::string content(
				(std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()
			);
			file.close();

			if (content.empty()) {
				return;
			}

			boost::json::value jsonDocument = boost::json::parse(content);

			if (!jsonDocument.is_object()) {
				std::cerr << "Config file is not a valid JSON object: " << configFilePath << '\n';
				return;
			}

			const boost::json::object &jsonKeyValuePairs = jsonDocument.as_object();
			configData.clear();

			for (const auto &pair : jsonKeyValuePairs) {
				if (pair.value().is_string()) {
					configData[pair.key()] = pair.value().as_string().c_str();
				}
			}
		} catch (const std::exception &e) {
			std::cerr << "Error parsing config file " << configFilePath << ". " << e.what() << '\n';
		}
	}

	void saveConfig()
	{
		try {
			boost::json::object jsonObj;
			std::lock_guard<std::mutex> lock(dataMutex);

			for (const auto &[key, value] : configData) {
				jsonObj[key] = value;
			}

			boost::json::value jsonDocument = jsonObj;

			std::string jsonDocumentStr = boost::json::serialize(jsonDocument);

			std::ofstream file(configFilePath);
			if (!file.is_open()) {
				std::cerr << "Error opening config file for writing. " << configFilePath << '\n';
				return;
			}

			file << jsonDocumentStr;
			file.close();

			isNeedSave = false;
		} catch (const std::exception &e) {
			std::cerr << "Error saving config to " << configFilePath << ". " << e.what() << '\n';
		}
	}

	void startSaveThread()
	{
		saveThread = std::jthread(&Config::saveWorker, this);
	}

	void stopSaveThread()
	{
		saveThread.request_stop();
	}

	void saveWorker(const std::stop_token &stopToken)
	{
		while (true) {
			std::unique_lock<std::mutex> lock(saveMutex);

			bool ret = saveCV.wait_for(lock, stopToken, saveConfigTimeout, [this]() {
				return isNeedSave;
			});

			if (ret) {
				if (isNeedSave) {
					saveConfig();
				}
			} else {
				break;
			}
		}
	}
};

} // namespace kvserver
