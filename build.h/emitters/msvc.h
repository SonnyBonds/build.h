#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "core/emitter.h"
#include "core/project.h"
#include "core/stringid.h"
#include "modules/command.h"
#include "modules/postprocess.h"
#include "modules/toolchain.h"
#include "toolchains/detected.h"
#include "util/string.h"

class MsvcEmitter : public Emitter
{
public:
    static MsvcEmitter instance;

    MsvcEmitter()
        : Emitter("msvc", "Generate Msvc project files.")
    {
    }

    virtual void emit(Environment& env) override
    {
        std::filesystem::create_directories(*targetPath);

        auto projects = env.collectProjects();

        auto configs = env.collectConfigs();

        for(auto project : projects)
        {
            auto outputName = emitProject(*targetPath, *project, configs, false);
            if(!outputName.empty())
            {
            }
        }
    }

private:
    static std::string emitProject(const std::filesystem::path& root, Project& project, std::vector<StringId> configs, bool generator)
    {
        auto resolvedProperties = project.resolve("", OperatingSystem::current());
        //resolved.dataDir = root;

#if TODO
        {
            // Avoiding range-based for loop here since it breaks
            // if a post processor adds more post processors. 
            auto postProcessors = resolved[PostProcess];
            for(size_t i = 0; i < postProcessors.size(); ++i)
            {
                postProcessors[i](project, resolved);
            }
        }
#endif

        if(!project.type.has_value())
        {
            return {};
        }

        if(project.name.empty())
        {
            throw std::runtime_error("Trying to emit project with no name.");
        }

        std::cout << "Emitting '" << project.name << "'\n";

        auto vcprojName = project.name + ".vcxproj";

        std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), root);

        {
            SimpleXmlWriter xml(root / vcprojName);
            {
                auto tag = xml.tag("Project", {
                    {"DefaultTargets", "Build"}, 
                    {"ToolsVersion", "16.0"}, 
                    {"xmlns", "http://schemas.microsoft.com/developer/msbuild/2003"}
                });
                {
                    auto tag = xml.tag("ItemGroup", {{"Label", "ProjectConfigurations"}});
                    for(auto& config : configs)
                    {
                        auto tag = xml.tag("ProjectConfiguration", {{"Include", config.cstr()}});
                        xml.shortTag("Configuration", {}, config.cstr());
                        xml.shortTag("Platform", {}, "x64");
                    }
                }
            }

            {
                auto tag = xml.tag("PropertyGroup", {{"Label", "Globals"}});
                
            }

            {
                auto tag = xml.tag("ItemGroup");
                for(auto& input : resolvedProperties.files)
                {
                    auto language = input.language != lang::Auto ? input.language : Language::getByPath(input.path);
                    if(language == lang::None)
                    {
                        continue;
                    }

                    xml.tag("ClCompile", {{"Include", (pathOffset / input.path).string()}});
                }
            }

#if 0

        auto& commands = resolved.commands;
        if(project.type == Command && commands.value().empty())
        {
            throw std::runtime_error("Command project '" + project.name + "' has no commands.");
        }

        std::vector<std::string> projectOutputs;

        const ToolchainProvider* toolchain = resolved.toolchain;
        if(!toolchain)
        {
            toolchain = defaultToolchain;
        }

        auto toolchainOutputs = toolchain->process(project, resolved, config, root);
        for(auto& output : toolchainOutputs)
        {
            projectOutputs.push_back((pathOffset / output).string());
        }

        std::string prologue;
        // TODO: Target platform
        /*if(windows)
        {
            prologue += "cmd /c ";
        }*/
        prologue += "cd \"$cwd\" && ";
        Msvc.rule("command", prologue + "$cmd", "$depfile", "", "$desc", generator);

        std::vector<std::string> generatorDep = { "_generator" };
        std::vector<std::string> emptyDep = {};

        for(auto& command : commands)
        {
            std::filesystem::path cwd = command.workingDirectory;
            if(cwd.empty())
            {
                cwd = ".";
            }
            std::string cwdStr = (pathOffset / cwd).string();

            std::vector<std::string> inputStrs;
            inputStrs.reserve(command.inputs.size());
            for(auto& path : command.inputs)
            {
                inputStrs.push_back((pathOffset / path).string());
            }

            std::vector<std::string> outputStrs;
            outputStrs.reserve(command.outputs.size());
            for(auto& path : command.outputs)
            {
                if(generator && path.extension() == ".Msvc")
                {
                    outputStrs.push_back(path.string());
                }
                else
                {
                    outputStrs.push_back((pathOffset / path).string());
                }
            }

            projectOutputs.insert(projectOutputs.end(), outputStrs.begin(), outputStrs.end());

            std::string depfileStr;
            if(!command.depFile.empty())
            {
                depfileStr = (pathOffset / command.depFile).string();
            }

            std::vector<std::pair<std::string_view, std::string_view>> variables;
            variables.push_back({"cmd", command.command});
            variables.push_back({"cwd", cwdStr});
            variables.push_back({"depfile", depfileStr});
            if(!command.description.empty())
            {
                variables.push_back({"desc", command.description});
            }
            Msvc.build(outputStrs, "command", inputStrs, {}, project.name == "_generator" ? emptyDep : generatorDep, variables);
        }

        if(!projectOutputs.empty())
        {
            Msvc.build({ project.name }, "phony", projectOutputs);
        }
#endif
        }

        return vcprojName;
    }

private:
    struct TagTerminator
    {
        ~TagTerminator()
        {
            indent -= 2;
            stream << str::padLeft("</" + tag + ">", indent) << "\n";
        }

        std::string tag;
        std::ofstream& stream;
        int& indent;
    };

    struct SimpleXmlWriter
    {
        std::ofstream stream;
        int indent = 0;

        SimpleXmlWriter(std::filesystem::path path)
            : stream(path)
        {
            stream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
        }

        ~SimpleXmlWriter()
        {
        }

        TagTerminator tag(std::string tag, const std::vector<std::pair<std::string, std::string>>& attributes = {})
        {
            stream << str::padLeft("<" + tag, indent);
            for(auto& attribute : attributes)
            {
                stream << " " << attribute.first << "=" << str::quote(attribute.second);
            }
            stream << ">\n";
            indent += 2;
            return {tag, stream, indent};
        }

        void shortTag(std::string tag, const std::vector<std::pair<std::string, std::string>>& attributes = {}, std::string content = {})
        {
            stream << str::padLeft("<" + tag, indent);
            for(auto& attribute : attributes)
            {
                stream << " " << attribute.first << "=" << str::quote(attribute.second);
            }
            stream << ">" << content << "</" << tag << ">\n";
        }
    };
};

MsvcEmitter MsvcEmitter::instance;
