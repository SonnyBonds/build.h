#pragma once

#include <cstdlib>
#include <assert.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "core/emitter.h"
#include "core/project.h"
#include "core/stringid.h"
#include "modules/command.h"
#include "modules/postprocess.h"
#include "modules/toolchain.h"
#include "toolchains/detected.h"
#include "util/cli.h"
#include "util/file.h"
#include "util/process.h"
#include "util/string.h"

struct PendingCommand;

class DirectBuilder : public Emitter
{
public:
    static DirectBuilder instance;

    cli::StringArgument selectedConfig{arguments, "config", "Specify a configuration to build."};

    DirectBuilder();

    virtual void emit(Environment& env) override;

private:
    static void collectCommands(std::vector<PendingCommand>& pendingCommands, const std::filesystem::path& root, Project& project, StringId config);
    static size_t runCommands(const std::vector<PendingCommand*>& commands, size_t maxConcurrentCommands);
    static std::vector<PendingCommand*> processCommands(std::vector<PendingCommand>& pendingCommands);
};
