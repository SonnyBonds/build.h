#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "core/option.h"
#include "core/os.h"
#include "core/stringid.h"
#include "modules/standardoptions.h"
#include "util/operators.h"

enum ProjectType
{
    Executable,
    StaticLib,
    SharedLib,
    Command
};

enum Transitivity
{
    Local,
    Public,
    PublicOnly
};

struct ConfigSelector
{
    ConfigSelector() {}
    ConfigSelector(StringId name)
        : name(name)
    {}

    ConfigSelector(Transitivity transitivity)
        : transitivity(transitivity)
    {}

    ConfigSelector(ProjectType projectType)
        : projectType(projectType)
    {}

    ConfigSelector(OperatingSystem targetOS)
        : targetOS(targetOS)
    {}

    std::optional<Transitivity> transitivity;
    std::optional<StringId> name;
    std::optional<ProjectType> projectType;
    std::optional<OperatingSystem> targetOS;

    bool operator <(const ConfigSelector& other) const
    {
        if(transitivity != other.transitivity) return transitivity < other.transitivity;
        if(projectType != other.projectType) return projectType < other.projectType;
        if(name != other.name) return name < other.name;
        if(targetOS != other.targetOS) return targetOS < other.targetOS;

        return false;
    }
};

ConfigSelector operator/(Transitivity a, ConfigSelector b)
{
    if(b.transitivity) throw std::invalid_argument("Transitivity was specified twice.");
    b.transitivity = a;

    return b;
}

ConfigSelector operator/(ProjectType a, ConfigSelector b)
{
    if(b.projectType) throw std::invalid_argument("Project type was specified twice.");
    b.projectType = a;

    return b;
}

ConfigSelector operator/(StringId a, ConfigSelector b)
{
    if(b.name) throw std::invalid_argument("Configuration name was specified twice.");
    b.name = a;

    return b;
}

ConfigSelector operator/(OperatingSystem a, ConfigSelector b)
{
    if(b.targetOS) throw std::invalid_argument("Configuration target operating system was specified twice.");
    b.targetOS = a;

    return b;
}

struct Project
{
    std::string name;
    std::optional<ProjectType> type;
    std::map<ConfigSelector, OptionCollection, std::less<>> configs;
    std::vector<Project*> links;

    Project(std::string name = {}, std::optional<ProjectType> type = {})
        : name(std::move(name)), type(type)
    {
    }

    OptionCollection resolve(std::optional<ProjectType> projectType, StringId configName, OperatingSystem targetOS)
    {
        auto options = internalResolve(projectType, configName, targetOS, true);
        options.deduplicate();
        return options;
    }

    OptionCollection& operator[](ConfigSelector selector)
    {
        return configs[selector];
    }

    template<typename T>
    T& operator[](Option<T> option)
    {
        return configs[{}][option];
    }

    void operator +=(const OptionCollection& collection)
    {
        configs[{}] += collection;
    }

    std::filesystem::path calcOutputPath(OptionCollection& resolvedOptions)
    {
        auto path = resolvedOptions[OutputPath];
        if(!path.empty())
        {
            return path;
        }

        auto stem = resolvedOptions[OutputStem];
        if(stem.empty())
        {
            stem = name;
        }

        return resolvedOptions[OutputDir] / (resolvedOptions[OutputPrefix] + stem + resolvedOptions[OutputSuffix] + resolvedOptions[OutputStem]);
    }

private:
    OptionCollection internalResolve(std::optional<ProjectType> projectType, StringId configName, OperatingSystem targetOS, bool local)
    {
        OptionCollection result;

        for(auto& link : links)
        {
            result.combine(link->internalResolve(projectType, configName, targetOS, false));
        }

        for(auto& entry : configs)
        {
            if(local)
            {
                if(entry.first.transitivity && entry.first.transitivity == PublicOnly) continue;
            }
            else
            {
                if(!entry.first.transitivity || entry.first.transitivity == Local) continue;
            }
            if(entry.first.projectType && entry.first.projectType != projectType) continue;
            if(entry.first.name && entry.first.name != configName) continue;
            if(entry.first.targetOS && entry.first.targetOS != targetOS) continue;

            result.combine(entry.second);
        }

        return result;
    }
};