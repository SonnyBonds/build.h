#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/emitter.h"
#include "core/project.h"
#include "core/stringid.h"
#include "modules/command.h"
#include "modules/postprocess.h"
#include "modules/toolchain.h"
#include "toolchains/detected.h"
#include "util/operators.h"
#include "util/process.h"
#include "util/file.h"

class DirectBuilder
{
public:
    static void emit(const EmitterArgs& args)
    {
        auto projects = args.projects;

        std::vector<std::filesystem::path> generatorDependencies;
        for(auto& project : projects)
        {
            generatorDependencies += (*project)[GeneratorDependencies];
            for(auto& entry : project->configs)
            {
                generatorDependencies += (*project)[GeneratorDependencies];
            }
        }

        auto buildOutput = std::filesystem::path(BUILD_FILE).replace_extension("");
        Project generator("_generator", Executable);
        generator[Features] += { "c++17", "optimize" };
        generator[IncludePaths] += BUILD_H_DIR;
        generator[OutputPath] = buildOutput;
        generator[Defines] += {
            "START_DIR=\\\"" START_DIR "\\\"",
            "BUILD_H_DIR=\\\"" BUILD_H_DIR "\\\"",
            "BUILD_DIR=\\\"" BUILD_DIR "\\\"",
            "BUILD_FILE=\\\"" BUILD_FILE "\\\"",
            "BUILD_ARGS=\\\"" BUILD_ARGS "\\\"",
        };
        generator[Files] += BUILD_FILE;

        generatorDependencies += buildOutput;
        generator[Commands] += { "\"" + (BUILD_DIR / buildOutput).string() + "\" " BUILD_ARGS, generatorDependencies, { }, START_DIR, {}, "Running build generator." };

        projects.push_back(&generator);

        projects = Emitter::discoverProjects(projects);

        std::vector<PendingCommand> pendingCommands;
        using It = decltype(pendingCommands.begin());

        for(auto project : projects)
        {
            build(pendingCommands, args.targetPath, *project, args.config);
        }

        struct PathHash
        {
            size_t operator()(const std::filesystem::path& path) const
            {
                return std::filesystem::hash_value(path);
            }   
        };

        std::unordered_map<std::filesystem::path, PendingCommand*, PathHash> commandMap;
        for(auto& command : pendingCommands)
        {
            for(auto& output : command.outputs)
            {
                commandMap[output] = &command;
            }
        }

        for(auto& command : pendingCommands)
        {
            command.dependencies.reserve(command.inputs.size());
            for(auto& input : command.inputs)
            {
                auto it = commandMap.find(input);
                if(it != commandMap.end())
                {
                    command.dependencies.push_back(it->second);
                }
            }
        }

        It next = pendingCommands.begin();
        std::vector<std::pair<PendingCommand*, int>> stack;
        stack.reserve(pendingCommands.size());
        std::vector<PendingCommand*> commands;
        commands.reserve(pendingCommands.size());
        while(next != pendingCommands.end() || !stack.empty())
        {
            PendingCommand* command;
            int depth = 0;
            if(stack.empty())
            {
                command = &*next;
                depth = command->depth;
                ++next;
                commands.push_back(command);
            }
            else
            {
                command = stack.back().first;
                depth = stack.back().second;
                stack.pop_back();
            }

            command->depth = depth;

            for(auto& dependency : command->dependencies)
            {
                if(dependency->depth < depth+1)
                {
                    stack += {dependency, depth+1};
                }
            }
        }

        std::sort(commands.begin(), commands.end(), [](auto a, auto b) { return a->depth > b->depth; });
        
        for(auto command : commands)
        {
            for(auto dependency : command->dependencies)
            {
                if(dependency->dirty)
                {
                    command->dirty = true;
                    break;
                }
            }
            if(command->dirty) continue;

            std::filesystem::file_time_type outputTime;
            outputTime = outputTime.max();
            std::error_code ec;
            for(auto& output : command->outputs)
            {
                outputTime = std::min(outputTime, std::filesystem::last_write_time(output, ec));
                if(ec)
                {
                    command->dirty = true;
                    break;
                }
            }
            if(command->dirty) continue;

            for(auto& input : command->inputs)
            {
                auto inputTime = std::filesystem::last_write_time(input, ec);
                if(ec || inputTime > outputTime)
                {
                    command->dirty = true;
                    break;
                }
            }
            if(command->dirty) continue;

            if(checkDeps(command->depFile, outputTime))
            {
                command->dirty = true;
            }            
        }

        commands.erase(std::remove_if(commands.begin(), commands.end(), [](auto command) { return !command->dirty; }), commands.end());

        std::string line;

        size_t count = 1;
        size_t firstPending = 0;
        for(auto command : commands)
        {
            std::cout << "\33[2K\r[" << count << "/" << commands.size() << "] " << command->desciption << std::flush;

            for(auto& output : command->outputs)
            {
                if(output.has_parent_path())
                {
                    std::filesystem::create_directories(output.parent_path());
                }
            }
            auto result = process::run(command->commandString + " 2>&1");
            if(result.exitCode != 0)
            {
                std::cout << "\n" << result.output << std::flush;
                throw std::runtime_error("Command returned " + std::to_string(result.exitCode));
            }

            ++count;
        }

        std::cout << "\n" << std::string(args.config) + ": " + std::to_string(commands.size()) << " targets rebuilt.";
        if(commands.size() == 0)
        {
            std::cout << " (Everything up to date.)";
        }
        std::cout << "\n";
    }

private:
    struct PendingCommand
    {
        std::vector<std::filesystem::path> inputs;
        std::vector<std::filesystem::path> outputs;
        std::filesystem::path depFile;
        std::string commandString;
        std::string desciption;
        int depth = 0;
        bool dirty = false;
        std::vector<PendingCommand*> dependencies;
    };

    static bool checkDeps(const std::filesystem::path& path, std::filesystem::file_time_type outputTime)
    {
        if(path.empty())
        {
            return false;
        }

        auto data = file::read(path);
        if(data.empty())
        {
            return true;
        }

        size_t pos = 0;
        auto skipWhitespace = [&](){
            while(pos < data.size())
            {
                char c = data[pos];
                if(!std::isspace(data[pos]) && data[pos] != '\\')
                {
                    break;
                }
                ++pos;
            }
        };

        auto readPath = [&](std::string& result){
            result.clear();
            bool escaped = false;
            while(pos < data.size())
            {
                char c = data[pos];
                if(c == '\\')
                {
                    if(escaped)
                    {
                        result += '\\';
                    }
                    escaped = true;
                    ++pos;
                    continue;
                }
                else if(std::isspace(c))
                {
                    if(escaped)
                    {
                        escaped = false;
                    }
                    else
                    {
                        return result;
                    }
                }
                else if(escaped)
                {
                    result += '\\';
                    escaped = false;
                }
                
                result += c;
                ++pos;
            }
            return result;
        };

        ++pos;

        bool scanningOutputs = true;
        std::string pathString;
        while(pos < data.size())
        {
            skipWhitespace();
            readPath(pathString);
            if(pathString.empty())
            {
                continue;
            }

            if(pathString.back() == ':')
            {
                scanningOutputs = false;
                continue;
            }

            if(scanningOutputs)
            {
                continue;
            }

            std::error_code ec;
            auto inputTime = std::filesystem::last_write_time(pathString, ec);
            if(ec || inputTime > outputTime)
            {
                return true;
            }
        }

        return false;
    }

    static void build(std::vector<PendingCommand>& pendingCommands, const std::filesystem::path& root, Project& project, StringId config)
    {
        auto resolved = project.resolve(project.type, config, OperatingSystem::current());
        resolved[DataDir] = root;

        {
            // Avoiding range-based for loop here since it breaks
            // if a post processor adds more post processors. 
            auto postProcessors = resolved[PostProcess];
            for(size_t i = 0; i < postProcessors.size(); ++i)
            {
                postProcessors[i](project, resolved);
            }
        }

        if(!project.type.has_value())
        {
            return;
        }

        if(project.name.empty())
        {
            throw std::runtime_error("Trying to build project with no name.");
        }

        std::filesystem::create_directories(root);
        std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), root);

        auto& commands = resolved[Commands];
        if(project.type == Command && commands.empty())
        {
            throw std::runtime_error("Command project '" + project.name + "' has no commands.");
        }

        const ToolchainProvider* toolchain = resolved[Toolchain];
        if(!toolchain)
        {
            toolchain = defaultToolchain;
        }

        auto toolchainOutputs = toolchain->process(project, resolved, config, {});

        std::string prologue;
        if(OperatingSystem::current() == Windows)
        {
            prologue += "cmd /c ";
        }
        prologue += "cd \"$cwd\" && ";

        pendingCommands.reserve(pendingCommands.size() + commands.size());
        for(auto& command : commands)
        {
            std::filesystem::path cwd = command.workingDirectory;
            if(cwd.empty())
            {
                cwd = ".";
            }
            std::string cwdStr = cwd.string();

            std::vector<std::string> inputStrs;
            inputStrs.reserve(command.inputs.size());
            for(auto& path : command.inputs)
            {
                inputStrs.push_back((pathOffset / path).string());
            }

            std::string depfileStr;
            if(!command.depFile.empty())
            {
                depfileStr = (pathOffset / command.depFile).string();
            }

            pendingCommands.push_back({
                command.inputs,
                command.outputs,
                command.depFile,
                "cd \"" + cwdStr + "\" && " + command.command,
                command.description,
            });
        }
    }

    static Emitters::Token installToken;
};
Emitters::Token DirectBuilder::installToken = Emitters::install({ "direct", &DirectBuilder::emit });
