#pragma once

#include <atomic>
#include <string>
#include <unordered_set>

// Launders strings into directly comparable pointers
struct StringId
{
    StringId() : _cstr("") {}
    StringId(const StringId& id) = default;
    StringId(const char* id) : StringId(get(id)) {}
    StringId(const std::string& id) : StringId(get(id)) {}
    StringId(std::string&& id) : StringId(get(std::move(id))) {}
    StringId(const std::string_view& id) : StringId(get(id)) {}

    bool empty() const
    {
        return _cstr == nullptr || _cstr[0] == 0;
    }

    const char* cstr() const
    {
        return _cstr;
    }

    operator const char*() const
    {
        return _cstr;
    }

    static size_t getStorageSize()
    {
        return getStorage().size();
    }

private:
    const char* _cstr;
    
    // Transparent lookup in unordered_set is a C++20 feature
    // so we'll have to make do with this... thing.
    struct StringStorage
    {
        StringStorage(std::string str)
            : str(str)
        {
            view = std::string_view(str);
        }

        StringStorage(std::string_view view)
            : view(view)
        {
        }

        StringStorage(StringStorage&& other)
        {
            str = std::move(other.str);
            view = str;
        }

        StringStorage(const StringStorage& other) = delete;

        bool operator==(const StringStorage& other) const
        {
            return view == other.view;
        }
        
        const char* c_str() const
        {
            return str.c_str();
        }

        std::string str;
        std::string_view view;
    };

    struct StringStorageHash
    {
        size_t operator()(const StringStorage& storage) const
        {
            return std::hash<std::string_view>{}(storage.view);
        }
    };

    static std::unordered_set<StringStorage, StringStorageHash>& getStorage()
    {
        static std::unordered_set<StringStorage, StringStorageHash> storage;
        return storage;
    }

    static StringId get(std::string_view str)
    {
        if(str.empty())
        {
            return StringId();
        }

        auto& storage = getStorage();
        auto it = storage.find(str);
        if(it == storage.end())
        {
            it = storage.insert(std::string(str)).first;
        }

        StringId result;
        result._cstr = it->c_str();
        return result;
    }

    static StringId get(std::string&& str)
    {
        if(str.empty())
        {
            return StringId();
        }

        auto& storage = getStorage();
        auto it = storage.find(str);
        if(it == storage.end())
        {
            it = storage.insert(std::move(str)).first;
        }

        StringId result;
        result._cstr = it->c_str();
        return result;
    }

    static StringId get(const std::string& str)
    {
        return get(std::string_view(str));
    }

    static StringId get(const char* str)
    {
        return get(std::string_view(str));
    }
};

template<>
struct std::hash<StringId>
{
    std::size_t operator()(StringId const& id) const
    {
        return std::hash<const void*>{}(id.cstr());
    }
};
