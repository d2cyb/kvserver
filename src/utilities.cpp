module;

#include <regex>
#include <string>
#include <variant>

export module kvserver.utilities;

export namespace kvserver {

auto trim(const std::string &targetString) -> std::string
{
	constexpr const char *whitespace { " \t\r\n\v\f" };
	if (targetString.empty()) {
		return targetString;
	}
	const auto first { targetString.find_first_not_of(whitespace) };
	if (first == std::string::npos) {
		return {};
	}
	const auto last { targetString.find_last_not_of(whitespace) };
	return targetString.substr(first, (last - first + 1));
}

struct CommandGet {
	std::string key;
};

struct CommandSet {
	std::string key;
	std::string value;
};

struct CommandUndefined { };

auto parseCommand(const std::string &command)
	-> std::variant<CommandGet, CommandSet, CommandUndefined>
{
	const static std::regex getRegex { R"(get\s+(\w+))" };
	const static std::regex setRegex { R"(set\s+(\w+)\s*=\s*(.+))" };

	auto trimCommand = trim(command);
	std::smatch matches;

	if (std::regex_match(trimCommand, matches, getRegex)) {
		if (matches.size() == 2) {
			return CommandGet { .key = matches[1].str() };
		}
	} else if (std::regex_match(trimCommand, matches, setRegex)) {
		if (matches.size() == 3) {
			return CommandSet { .key = matches[1].str(), .value = trim(matches[2].str()) };
		}
	}

	return CommandUndefined {};
}

} // namespace kvserver
