// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <regex>
#include <utility>
#include <vector>
#include <fmt/format.h>
#include "common/logging/log.h"
#include "common/param_package.h"
#include "common/string_util.h"

namespace Common {

constexpr char KEY_VALUE_SEPARATOR = ':';
constexpr char PARAM_SEPARATOR = ',';
constexpr char LIST_SEPARATOR = '|';

// Matches text enclosed in square brackets.
const std::regex LIST_MATCH(R"(\[(?:\[??[^\[]*?\]))");

constexpr char ESCAPE_CHARACTER = '$';
constexpr char KEY_VALUE_SEPARATOR_ESCAPE[] = "$0";
constexpr char PARAM_SEPARATOR_ESCAPE[] = "$1";
constexpr char ESCAPE_CHARACTER_ESCAPE[] = "$2";

/// A placeholder for empty param packages to avoid empty strings
/// (they may be recognized as "not set" by some frontend libraries like qt)
constexpr char EMPTY_PLACEHOLDER[] = "[empty]";

ParamPackage::ParamPackage(const std::string& serialized) {
    if (serialized == EMPTY_PLACEHOLDER) {
        return;
    }

    std::vector<std::string> lookup{};
    std::string s = PlaceholderifyData(serialized, lookup);

    std::vector<std::string> pairs;
    Common::SplitString(s, PARAM_SEPARATOR, pairs);

    for (const std::string& pair : pairs) {
        std::vector<std::string> key_value;
        Common::SplitString(pair, KEY_VALUE_SEPARATOR, key_value);
        if (key_value.size() != 2) {
            LOG_ERROR(Common, "invalid key pair {}", pair);
            continue;
        }

        for (std::string& part : key_value) {
            part = Common::ReplaceAll(part, KEY_VALUE_SEPARATOR_ESCAPE, {KEY_VALUE_SEPARATOR});
            part = Common::ReplaceAll(part, PARAM_SEPARATOR_ESCAPE, {PARAM_SEPARATOR});
            part = Common::ReplaceAll(part, ESCAPE_CHARACTER_ESCAPE, {ESCAPE_CHARACTER});
        }

        Set(key_value[0], std::move(key_value[1]));
    }

    for (auto& [key, value] : data) {
        ReplacePlaceholders(value, lookup);
    }
}

ParamPackage::ParamPackage(std::initializer_list<DataType::value_type> list) : data(list) {}

std::string ParamPackage::Serialize() const {
    if (data.empty())
        return EMPTY_PLACEHOLDER;

    std::string result;

    for (const auto& pair : data) {
        std::array<std::string, 2> key_value{{pair.first, pair.second}};
        for (std::string& part : key_value) {
            part = Common::ReplaceAll(part, {ESCAPE_CHARACTER}, ESCAPE_CHARACTER_ESCAPE);
            part = Common::ReplaceAll(part, {PARAM_SEPARATOR}, PARAM_SEPARATOR_ESCAPE);
            part = Common::ReplaceAll(part, {KEY_VALUE_SEPARATOR}, KEY_VALUE_SEPARATOR_ESCAPE);
        }
        result += key_value[0] + KEY_VALUE_SEPARATOR + key_value[1] + PARAM_SEPARATOR;
    }

    result.pop_back(); // discard the trailing PARAM_SEPARATOR
    return result;
}

// Getters

std::string ParamPackage::Get(const std::string& key, const std::string& default_value) const {
    auto pair = data.find(key);
    if (pair == data.end()) {
        LOG_DEBUG(Common, "key {} not found", key);
        return default_value;
    }

    return pair->second;
}

std::vector<std::string> ParamPackage::Get(const std::string& key,
                                           const std::vector<std::string>& default_value) const {
    auto pair = data.find(key);
    if (pair == data.end()) {
        LOG_DEBUG(Common, "key {} not found", key);
        return default_value;
    }

    std::string val = "";

    if (pair->second.front() == '[' && pair->second.back() == ']') {
        val = pair->second.substr(1, pair->second.size() - 2);
    } else {
        LOG_ERROR(Common, "failed to convert {} to vector", pair->second);
        return default_value;
    }

    std::vector<std::string> vec;
    Common::SplitString(val, LIST_SEPARATOR, vec);
    return vec;
}

int ParamPackage::Get(const std::string& key, int default_value) const {
    auto pair = data.find(key);
    if (pair == data.end()) {
        LOG_DEBUG(Common, "key {} not found", key);
        return default_value;
    }

    try {
        return std::stoi(pair->second);
    } catch (const std::logic_error&) {
        LOG_ERROR(Common, "failed to convert {} to int", pair->second);
        return default_value;
    }
}

std::vector<int> ParamPackage::Get(const std::string& key,
                                   const std::vector<int>& default_value) const {
    auto vec0 = Get(key, std::vector<std::string>{});
    if (vec0.empty()) {
        return default_value;
    }
    std::vector<int> vec{};
    for (auto& str : vec0) {
        try {
            vec.emplace_back(std::stoi(str));
        } catch (const std::logic_error&) {
            LOG_ERROR(Common, "failed to convert {} to int", str);
            return default_value;
        }
    }
    return vec;
}

float ParamPackage::Get(const std::string& key, float default_value) const {
    auto pair = data.find(key);
    if (pair == data.end()) {
        LOG_DEBUG(Common, "key {} not found", key);
        return default_value;
    }

    try {
        return std::stof(pair->second);
    } catch (const std::logic_error&) {
        LOG_ERROR(Common, "failed to convert {} to float", pair->second);
        return default_value;
    }
}

std::vector<float> ParamPackage::Get(const std::string& key,
                                     const std::vector<float>& default_value) const {
    auto vec0 = Get(key, std::vector<std::string>{});
    if (vec0.empty()) {
        return default_value;
    }
    std::vector<float> vec{};
    for (auto& str : vec0) {
        try {
            vec.emplace_back(std::stof(str));
        } catch (const std::logic_error&) {
            LOG_ERROR(Common, "failed to convert {} to float", str);
            return default_value;
        }
    }
    return vec;
}

ParamPackage ParamPackage::Get(const std::string& key, const ParamPackage& default_value) const {
    auto pair = data.find(key);
    if (pair == data.end()) {
        LOG_DEBUG(Common, "key {} not found", key);
        return default_value;
    }

    std::string val = "";

    if (pair->second.front() == '[' && pair->second.back() == ']') {
        val = pair->second.substr(1, pair->second.size() - 2);
        // Check that the data is actually a ParamPackage to prevent sending garbage data.
        // Placeholderify is necessary becuase we only want to check the outer layer.
        std::vector<std::string> lookup{};
        auto chk = PlaceholderifyData(val, lookup);
        if (chk.find({LIST_SEPARATOR}) != std::string::npos) {
            LOG_ERROR(Common, "{} is a vector, not a ParamPackage", pair->second);
            return default_value;
        }
        if (chk.find({PARAM_SEPARATOR}) == std::string::npos) {
            LOG_ERROR(Common, "{} is a unit vector of a primitive type, not a ParamPackage",
                      pair->second);
            return default_value;
        }
    } else {
        LOG_ERROR(Common, "{} is not a ParamPackage", pair->second);
        return default_value;
    }

    return ParamPackage{val};
}

std::vector<ParamPackage> ParamPackage::Get(const std::string& key,
                                            const std::vector<ParamPackage>& default_value) const {
    auto pair = data.find(key);
    if (pair == data.end()) {
        LOG_DEBUG(Common, "key {} not found", key);
        return default_value;
    }

    std::string val = "";

    std::vector<std::string> lookup{};
    if (pair->second.front() == '[' && pair->second.back() == ']') {
        val = pair->second.substr(1, pair->second.size() - 2);
        // Check that the data is actually a ParamPackage to prevent sending garbage data.
        // Placeholderify is necessary becuase we only want to check the outer layer.
        val = PlaceholderifyData(val, lookup);
        if (val.find({PARAM_SEPARATOR}) == std::string::npos) {
            LOG_ERROR(Common, "{} is a vector of a primitive type, not a ParamPackage",
                      pair->second);
            return default_value;
        }
    } else {
        LOG_ERROR(Common, "{} is not a vector", pair->second);
        return default_value;
    }

    std::vector<std::string> vec{};
    Common::SplitString(val, LIST_SEPARATOR, vec);
    std::for_each(vec.begin(), vec.end(),
                  [&lookup, this](std::string& in) { ReplacePlaceholders(in, lookup); });
    std::vector<ParamPackage> packs{};
    std::transform(vec.begin(), vec.end(), std::back_inserter(packs),
                   [](std::string& in) { return ParamPackage(in); });
    return packs;
}

// Setters

void ParamPackage::Set(const std::string& key, std::string value) {
    data.insert_or_assign(key, std::move(value));
}

void ParamPackage::Set(const std::string& key, std::vector<std::string> value) {
    data.insert_or_assign(key, fmt::format("[{}]", fmt::join(value, std::string{LIST_SEPARATOR})));
}

void ParamPackage::Set(const std::string& key, int value) {
    data.insert_or_assign(key, std::to_string(value));
}

void ParamPackage::Set(const std::string& key, std::vector<int> value) {
    data.insert_or_assign(key, fmt::format("[{}]", fmt::join(value, std::string{LIST_SEPARATOR})));
}

void ParamPackage::Set(const std::string& key, float value) {
    data.insert_or_assign(key, std::to_string(value));
}

void ParamPackage::Set(const std::string& key, std::vector<float> value) {
    data.insert_or_assign(key, fmt::format("[{}]", fmt::join(value, std::string{LIST_SEPARATOR})));
}

void ParamPackage::Set(const std::string& key, ParamPackage value) {
    data.insert_or_assign(key, std::move(value.Serialize()));
}

void ParamPackage::Set(const std::string& key, std::vector<ParamPackage> value) {
    data.insert_or_assign(key, fmt::format("[{}]", fmt::join(value, std::string{LIST_SEPARATOR})));
}

// Other methods

std::ostream& operator<<(std::ostream& os, const ParamPackage& p) {
    return os << p.Serialize();
}

bool ParamPackage::Has(const std::string& key) const {
    return data.find(key) != data.end();
}

void ParamPackage::Erase(const std::string& key) {
    data.erase(key);
}

void ParamPackage::Clear() {
    data.clear();
}

ParamPackage::DataType::iterator ParamPackage::begin() {
    return data.begin();
}

ParamPackage::DataType::const_iterator ParamPackage::begin() const {
    return data.begin();
}

ParamPackage::DataType::iterator ParamPackage::end() {
    return data.end();
}

ParamPackage::DataType::const_iterator ParamPackage::end() const {
    return data.end();
}

// Static helper methods

std::string ParamPackage::PlaceholderifyData(const std::string& str,
                                             std::vector<std::string>& lookup) {
    std::smatch m;
    std::string s = str;
    while (std::regex_search(s, m, LIST_MATCH)) {
        for (auto x : m) {
            s.replace(s.find(x.str()), x.length(), "##" + std::to_string(lookup.size()));
            lookup.emplace_back(x.str());
        }
    }
    return s;
}

void ParamPackage::ReplacePlaceholders(std::string& str, const std::vector<std::string>& lookup) {
    std::smatch m;
    while (std::regex_search(str, m, std::regex(R"(##([\d]+))"))) {
        for (auto x : m) {
            str.replace(str.find("##" + x.str()), 2 + x.length(), lookup[std::stoi(x.str())]);
        }
    }
}

std::string ParamPackage::ReplacePlaceholders(const std::string& str,
                                              const std::vector<std::string>& lookup) {
    std::string s = str;
    ReplacePlaceholders(s, lookup);
    return s;
}

} // namespace Common

template <>
struct fmt::formatter<Common::ParamPackage> {
    constexpr auto parse(fmt::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const Common::ParamPackage& p, FormatContext& ctx) {
        return format_to(ctx.out(), "{}", p.Serialize());
    }
};
