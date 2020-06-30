// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace Common {

/// A string-based key-value container supporting serializing to and deserializing from a string
class ParamPackage {
public:
    using DataType = std::map<std::string, std::string>;

    ParamPackage() = default;
    explicit ParamPackage(const std::string& serialized);
    ParamPackage(std::initializer_list<DataType::value_type> list);
    ParamPackage(const ParamPackage& other) = default;
    ParamPackage(ParamPackage&& other) = default;

    ParamPackage& operator=(const ParamPackage& other) = default;
    ParamPackage& operator=(ParamPackage&& other) = default;

    std::string Serialize() const;

    // Getters
    std::string Get(const std::string& key, const std::string& default_value) const;
    std::vector<std::string> Get(const std::string& key,
                                 const std::vector<std::string>& default_value) const;
    int Get(const std::string& key, int default_value) const;
    std::vector<int> Get(const std::string& key, const std::vector<int>& default_value) const;
    float Get(const std::string& key, float default_value) const;
    std::vector<float> Get(const std::string& key, const std::vector<float>& default_value) const;
    ParamPackage Get(const std::string& key, const ParamPackage& default_value) const;
    std::vector<ParamPackage> Get(const std::string& key,
                                  const std::vector<ParamPackage>& default_value) const;

    // Setters
    void Set(const std::string& key, std::string value);
    void Set(const std::string& key, std::vector<std::string> value);
    void Set(const std::string& key, int value);
    void Set(const std::string& key, std::vector<int> value);
    void Set(const std::string& key, float value);
    void Set(const std::string& key, std::vector<float> value);
    void Set(const std::string& key, ParamPackage value);
    void Set(const std::string& key, std::vector<ParamPackage> value);

    // Other methods
    bool Has(const std::string& key) const;
    void Erase(const std::string& key);
    void Clear();

    // For range-based for
    DataType::iterator begin();
    DataType::const_iterator begin() const;
    DataType::iterator end();
    DataType::const_iterator end() const;

private:
    DataType data;

    /**
     * Replace complex data structures with placeholders, and generate the lookup table.
     * The lookup table is emptied before being used.
     */
    static std::string PlaceholderifyData(const std::string& str, std::vector<std::string>& lookup);

    /**
     * Replace placeholders (in-place) with original data obtained from a lookup table.
     */
    static void ReplacePlaceholders(std::string& str, const std::vector<std::string>& lookup);

    /**
     * Replace placeholders with original data obtained from a lookup table and returns the
     * resultant string.
     */
    static std::string ReplacePlaceholders(const std::string& str,
                                           const std::vector<std::string>& lookup);
};

} // namespace Common
