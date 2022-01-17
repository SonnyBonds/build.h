#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/option.h"
#include "core/project.h"

struct ToolchainProvider
{
    std::string name;
    ToolchainProvider(std::string name) : name(std::move(name)) {}

    virtual std::string getCompiler(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset) const = 0;
    virtual std::string getCommonCompilerFlags(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset) const = 0;
    virtual std::string getCompilerFlags(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset, const std::string& input, const std::string& output) const = 0;

    virtual std::string getLinker(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset) const = 0;
    virtual std::string getCommonLinkerFlags(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset) const = 0;
    virtual std::string getLinkerFlags(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset, const std::vector<std::string>& inputs, const std::string& output) const = 0;

    virtual std::vector<std::filesystem::path> process(Project& project, OptionCollection& resolvedOptions, StringId config, const std::filesystem::path& workingDir) const = 0;
};

Option<ToolchainProvider*> Toolchain{"Toolchain"};

struct Toolchains
{
    using Token = int;

    static Token install(ToolchainProvider* toolchain)
    {
        getToolchains().push_back(toolchain);
        return {};
    }

    static const std::vector<const ToolchainProvider*>& list()
    {
        return getToolchains();
    }

private:
    static std::vector<const ToolchainProvider*>& getToolchains()
    {
        static std::vector<const ToolchainProvider*> toolchains;
        return toolchains;
    }
};