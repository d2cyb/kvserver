module;

#include <atomic>
#include <boost/json.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <shared_mutex>
#include <string>
#include <syncstream>
#include <thread>
#include <unordered_map>
#include <utility>

export module kvserver.config;

namespace kvserver {

using std::atomic;
using std::string;

const uint32_t STORE_FILE_INTERVAL_MILLISECONDS = 5 * 1000;

export struct ConfigData {
    string value;
    uint64_t reads { 0 };
    uint64_t writes { 0 };
};

struct ConfigDataAtomic {
    string value;
    atomic<uint64_t> reads { 0 };
    atomic<uint64_t> writes { 0 };

    static auto parse(boost::property_tree::ptree const &from) -> ConfigData
    {
        return ConfigData { .value  = from.get<string>("value"),
                            .reads  = from.get<uint64_t>("reads"),
                            .writes = from.get<uint64_t>("writes") };
    }
};

export class Config {
private:
    string configFilePath;

    std::unordered_map<string, ConfigDataAtomic> configData;
    mutable std::shared_timed_mutex dataMutex;

    const std::chrono::milliseconds saveConfigTimeout;
    std::atomic<bool> isNeedSave { false };
    std::jthread saveThread;

public:
    explicit Config(string configPath)
        : configFilePath(std::move(configPath))
        , configData {}
        , saveConfigTimeout { std::chrono::milliseconds(STORE_FILE_INTERVAL_MILLISECONDS) }
    {
        loadConfig();
        saveThread = std::jthread(std::bind_front(&Config::saveWorker, this));
    }

    ~Config()
    {
        saveThread.request_stop();
        saveConfig();
    }

    Config(const Config &)                     = delete;
    auto operator=(const Config &) -> Config & = delete;
    Config(Config &&)                          = delete;
    auto operator=(Config &&) -> Config &      = delete;

    auto get(const string &key) -> std::optional<ConfigData>
    {
        std::shared_lock<std::shared_timed_mutex> recentCommandReaderLock(dataMutex);

        if (auto it = configData.find(key); it != configData.end()) {
            it->second.reads++;
            return ConfigData { .value  = it->second.value,
                                .reads  = it->second.reads,
                                .writes = it->second.writes };
        }

        return {};
    }

    auto set(const string &key, const string &value) -> ConfigData
    {
        std::scoped_lock<std::shared_timed_mutex> recentCommandWriterLock(dataMutex);
        ConfigData config { .value = value, .reads = 0, .writes = 1 };

        if (auto it = configData.find(key); it != configData.end()) {
            it->second.value = value;
            it->second.writes++;
            config.reads  = it->second.reads;
            config.writes = it->second.writes;
        } else {
            configData[key].value  = config.value;
            configData[key].reads  = config.reads;
            configData[key].writes = config.writes;
        }

        isNeedSave = true;
        return config;
    }

private:
    void loadConfig()
    {
        if (!std::filesystem::exists(configFilePath)) {
            std::osyncstream(std::cerr)
                << "Config file not found, creating new one: " << configFilePath << std::endl;
            return;
        }

        try {
            std::scoped_lock<std::shared_timed_mutex> recentCommandWriterLock(dataMutex);

            std::ifstream file(configFilePath);
            boost::property_tree::ptree jsonDocument;
            boost::property_tree::read_json(file, jsonDocument);
            file.close();

            if (jsonDocument.empty()) {
                return;
            }

            for (const auto &pair : jsonDocument) {
                auto parsedValue              = ConfigDataAtomic::parse(pair.second);
                configData[pair.first].value  = parsedValue.value;
                configData[pair.first].reads  = parsedValue.reads;
                configData[pair.first].writes = parsedValue.writes;
            }

        } catch (const std::exception &e) {
            std::osyncstream(std::cerr)
                << "Error parsing config file " << configFilePath << ". " << e.what() << std::endl;
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

            std::scoped_lock<std::shared_timed_mutex> recentCommandWriterLock(dataMutex);
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

            string jsonDocumentStr = boost::json::serialize(jsonDocument);

            std::ofstream file(configFilePath);
            if (!file.is_open()) {
                std::osyncstream(std::cerr)
                    << "Error opening config file for writing. " << configFilePath << std::endl;
                return;
            }

            file << jsonDocumentStr;
            file.close();

            isNeedSave = false;
        } catch (const std::exception &e) {
            std::osyncstream(std::cerr)
                << "Error saving config to " << configFilePath << ". " << e.what() << std::endl;
        }
    }
};

} // namespace kvserver
