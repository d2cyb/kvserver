module;

#include <boost/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <syncstream>
#include <thread>
#include <unordered_map>
#include <utility>

export module kvserver.config;

namespace kvserver {

const uint32_t STORE_FILE_INTERVAL_MILLISECONDS = 5 * 1000;

export struct ConfigData {
	std::string value;
	uint64_t reads { 0 };
	uint64_t writes { 0 };
};

export class Config {
private:
	std::string configFilePath;

	std::unordered_map<std::string, ConfigData> configData;
	mutable std::mutex dataMutex;

	const std::chrono::milliseconds saveConfigTimeout;
	std::atomic<bool> isNeedSave { false };
	std::jthread saveThread;

public:
	explicit Config(std::string configFilePath)
		: configFilePath(std::move(configFilePath))
		, configData {}
		, saveConfigTimeout { std::chrono::milliseconds(STORE_FILE_INTERVAL_MILLISECONDS) }
	{
		loadConfig();
		saveThread = std::jthread(&Config::saveWorker, this);
	}

	~Config()
	{
		saveThread.request_stop();
		saveConfig();
	}

	Config(const Config &)					   = delete;
	auto operator=(const Config &) -> Config & = delete;
	Config(Config &&)						   = delete;
	auto operator=(Config &&) -> Config &	   = delete;

	auto get(const std::string &key) -> std::optional<ConfigData>
	{
		std::lock_guard<std::mutex> lock(dataMutex);

		auto it = configData.find(key);
		if (it != configData.end()) {
			it->second.reads++;
			return it->second;
		}

		return {};
	}

	auto set(const std::string &key, const std::string &value) -> ConfigData
	{
		std::lock_guard<std::mutex> lock(dataMutex);
		ConfigData config { .value = value, .reads = 0, .writes = 1 };

		if (auto it = configData.find(key); it != configData.end()) {
			it->second.value = value;
			it->second.writes++;
			config.reads  = it->second.reads;
			config.writes = it->second.writes;
		} else {
			configData.emplace(key, config);
		}

		isNeedSave = true;
		return config;
	}

private:
	void loadConfig()
	{
		if (!std::filesystem::exists(configFilePath)) {
			std::osyncstream(std::cerr)
				<< "Config file not found, creating new one: " << configFilePath << '\n';
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
				std::osyncstream(std::cerr)
					<< "Config file is not a valid JSON object: " << configFilePath << '\n';
				return;
			}

			const boost::json::object &jsonKeyValuePairs = jsonDocument.as_object();
			configData.clear();

			for (const auto &pair : jsonKeyValuePairs) {
				if (pair.value().is_object()) {
					auto configValueObj = pair.value().get_object();

					configData.emplace(
						pair.key(),
						ConfigData { .value	 = configValueObj.at("value").as_string().c_str(),
									 .reads	 = configValueObj.at("reads").is_uint64()
										  ? configValueObj.at("reads").as_uint64()
										  : 0,
									 .writes = configValueObj.at("writes").is_uint64()
										 ? configValueObj.at("writes").as_uint64()
										 : 0 }
					);
				}
			}
		} catch (const std::exception &e) {
			std::osyncstream(std::cerr)
				<< "Error parsing config file " << configFilePath << ". " << e.what() << '\n';
		}
	}

	void saveWorker(const std::stop_token stopToken)
	{
		while (not stopToken.stop_requested()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(saveConfigTimeout));
			saveConfig();
		}
	}

	void saveConfig()
	{
		try {
			boost::json::object jsonObj;

			std::lock_guard<std::mutex> lock(dataMutex);
			if (isNeedSave == false) {
				return;
			}

			for (const auto &[key, value] : configData) {
				jsonObj[key] = boost::json::object { {
				  { "value", value.value },
				  { "reads", value.reads },
				  { "writes", value.writes },
				} };
			}

			boost::json::value jsonDocument = jsonObj;

			std::string jsonDocumentStr = boost::json::serialize(jsonDocument);

			std::ofstream file(configFilePath);
			if (!file.is_open()) {
				std::osyncstream(std::cerr)
					<< "Error opening config file for writing. " << configFilePath << '\n';
				return;
			}

			file << jsonDocumentStr;
			file.close();

			isNeedSave = false;
		} catch (const std::exception &e) {
			std::osyncstream(std::cerr)
				<< "Error saving config to " << configFilePath << ". " << e.what() << '\n';
		}
	}
};

} // namespace kvserver
