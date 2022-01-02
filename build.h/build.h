#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <set>
#include <map>
#include <vector>
#include <sstream>
#include <array>
#include <unordered_set>
#include <functional>

// We're messing with the namespaces and various operator overloads and stuff
// in this header which typically is bad form, but I've chosen to allow this 
// header to dictate the whole "environment" since the build files typically 
// form a very isolated context

namespace fs = std::filesystem;

#if _WINDOWS
static bool windows = true;
#else
static bool windows = false;
#endif

struct Project;
struct ProjectConfig;

using PostProcessor = void(*)(Project& project, ProjectConfig& resolvedConfig);


struct BundleEntry
{
    fs::path source;
    fs::path target;

    bool operator <(const BundleEntry& other) const
    {
        {
            int v = source.compare(other.source);
            if(v != 0) return v < 0;
        }

        return target < other.target;
    }

    bool operator ==(const BundleEntry& other) const
    {
        return source == other.source &&
               target == other.target;
    }
};

template<>
struct std::hash<BundleEntry>
{
    std::size_t operator()(BundleEntry const& entry) const
    {
        std::size_t h = std::hash<std::string>{}(entry.source);
        h = h ^ (std::hash<std::string>{}(entry.target) << 1);
        return h;
    }
};

struct CommandEntry
{
    std::string command;
    std::vector<fs::path> outputs;
    fs::path workingDirectory;
    bool generator = false;

    bool operator ==(const CommandEntry& other) const
    {
        return command == other.command &&
               outputs == other.outputs &&
               generator == other.generator;
    }
};

template<>
struct std::hash<CommandEntry>
{
    std::size_t operator()(CommandEntry const& command) const
    {
        std::size_t h = std::hash<std::string>{}(command.command);
        for(auto& output : command.outputs)
        {
            h = h ^ (fs::hash_value(output) << 1);
        }
        h = h ^ (fs::hash_value(command.workingDirectory) << 1);
        return h;
    }
};

enum ProjectType
{
    Executable,
    StaticLib,
    SharedLib,
    Command
};

template<typename T>
struct Option
{
    typedef T Type;
    const char* id;

    bool operator <(const Option<T>& other) const
    {
        return id < other.id;
    }
};

enum Transitivity
{
    Local,
    Public,
    PublicOnly
};

struct ConfigSelector
{
    constexpr ConfigSelector(std::string_view name)
        : name(name)
    {}

    constexpr ConfigSelector(const char* name)
        : name(name)
    {}

    constexpr ConfigSelector(Transitivity transitivity)
        : transitivity(transitivity)
    {}

    constexpr ConfigSelector(ProjectType projectType)
        : projectType(projectType)
    {}

    std::optional<Transitivity> transitivity;
    std::optional<std::string_view> name;
    std::optional<ProjectType> projectType;

    bool operator <(const ConfigSelector& other) const
    {
        if(transitivity != other.transitivity) return transitivity < other.transitivity;
        if(projectType != other.projectType) return projectType < other.projectType;
        if(name != other.name) return name < other.name;

        return false;
    }
};

constexpr ConfigSelector operator/(Transitivity a, ConfigSelector b)
{
    // Can these throw compile time?
    if(b.transitivity) throw std::invalid_argument("Transitivity was specified twice.");
    b.transitivity = a;

    return b;
}

constexpr ConfigSelector operator/(ProjectType a, ConfigSelector b)
{
    // Can these throw compile time?
    if(b.projectType) throw std::invalid_argument("Project type was specified twice.");
    b.projectType = a;

    return b;
}

constexpr ConfigSelector operator/(std::string_view a, ConfigSelector b)
{
    // Can these throw compile time?
    if(b.name) throw std::invalid_argument("Configuration name was specified twice.");
    b.name = std::move(a);

    return b;
}

Option<std::string> Platform{"Platform"};
Option<std::vector<fs::path>> IncludePaths{"IncludePaths"};
Option<std::vector<fs::path>> Files{"Files"};
Option<std::vector<fs::path>> Dependencies{"Dependencies"};
Option<std::vector<fs::path>> Libs{"Libs"};
Option<std::vector<std::string>> Defines{"Defines"};
Option<std::vector<std::string>> Features{"Features"};
Option<std::vector<std::string>> Frameworks{"Frameworks"};
Option<std::vector<std::string>> ClangCppFlags{"ClangCppFlags"};
Option<std::vector<std::string>> ClangLinkFlags{"ClangLinkFlags"};
Option<std::vector<BundleEntry>> BundleContents{"BundleContents"};
Option<fs::path> OutputDir{"OutputDir"};
Option<std::string> OutputStem{"OutputStem"};
Option<std::string> OutputExtension{"OutputExtension"};
Option<std::string> OutputPrefix{"OutputPrefix"};
Option<std::string> OutputSuffix{"OutputSuffix"};
Option<fs::path> OutputPath{"OutputPath"};
Option<fs::path> BuildPch{"BuildPch"};
Option<fs::path> ImportPch{"ImportPch"};
Option<std::vector<PostProcessor>> PostProcess{"PostProcess"};
Option<std::vector<CommandEntry>> Commands{"Commands"};

// TODO: This whole thing can probably be templated and move better but I got lost in overload ambiguities and whatnot

std::vector<std::string>& operator +=(std::vector<std::string>& s, std::string other) {
    s.push_back(std::move(other));
    return s;
}
std::vector<std::string>& operator +=(std::vector<std::string>& s, std::initializer_list<std::string> other) {
    s.insert(s.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    return s;
}
std::vector<std::string>& operator +=(std::vector<std::string>& s, std::vector<std::string> other) {
    s.insert(s.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    return s;
}


std::vector<Project*>& operator +=(std::vector<Project*>& s, Project* other) {
    s.push_back(other);
    return s;
}
std::vector<Project*>& operator +=(std::vector<Project*>& s, std::initializer_list<Project*> other) {
    s.insert(s.end(), other.begin(), other.end());
    return s;
}
std::vector<Project*>& operator +=(std::vector<Project*>& s, std::vector<Project*> other) {
    s.insert(s.end(), other.begin(), other.end());
    return s;
}


std::vector<PostProcessor>& operator +=(std::vector<PostProcessor>& s, PostProcessor other) {
    s.push_back(other);
    return s;
}
std::vector<PostProcessor>& operator +=(std::vector<PostProcessor>& s, std::initializer_list<PostProcessor> other) {
    s.insert(s.end(), other.begin(), other.end());
    return s;
}
std::vector<PostProcessor>& operator +=(std::vector<PostProcessor>& s, std::vector<PostProcessor> other) {
    s.insert(s.end(), other.begin(), other.end());
    return s;
}


std::vector<fs::path>& operator +=(std::vector<fs::path>& s, fs::path other) {
    s.push_back(std::move(other));
    return s;
}
std::vector<fs::path>& operator +=(std::vector<fs::path>& s, std::initializer_list<fs::path> other) {
    s.insert(s.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    return s;
}
std::vector<fs::path>& operator +=(std::vector<fs::path>& s, std::vector<fs::path> other) {
    s.insert(s.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    return s;
}
std::vector<fs::path>& operator +=(std::vector<fs::path>& s, std::initializer_list<std::string> other) {
    s.insert(s.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    return s;
}
std::vector<fs::path>& operator +=(std::vector<fs::path>& s, std::vector<std::string> other) {
    s.insert(s.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    return s;
}

std::vector<BundleEntry>& operator +=(std::vector<BundleEntry>& s, BundleEntry other) {
    s.push_back(other);
    return s;
}
std::vector<BundleEntry>& operator +=(std::vector<BundleEntry>& s, std::initializer_list<BundleEntry> other) {
    s.insert(s.end(), other.begin(), other.end());
    return s;
}
std::vector<BundleEntry>& operator +=(std::vector<BundleEntry>& s, std::vector<BundleEntry> other) {
    s.insert(s.end(), other.begin(), other.end());
    return s;
}

std::vector<CommandEntry>& operator +=(std::vector<CommandEntry>& s, CommandEntry other) {
    s.push_back(other);
    return s;
}
std::vector<CommandEntry>& operator +=(std::vector<CommandEntry>& s, std::initializer_list<CommandEntry> other) {
    s.insert(s.end(), other.begin(), other.end());
    return s;
}
std::vector<CommandEntry>& operator +=(std::vector<CommandEntry>& s, std::vector<CommandEntry> other) {
    s.insert(s.end(), other.begin(), other.end());
    return s;
}

template<typename T>
struct OptionHash
{
    size_t operator()(const T& a) const
    { 
        return std::hash<T>()(a);
    };
};

template<>
struct OptionHash<fs::path>
{
    size_t operator()(const fs::path& a) const
    { 
        return fs::hash_value(a);
    };
};

struct OptionStorage
{
    typedef std::unique_ptr<void, void(*)(const void*)> Data;

    OptionStorage()
        : _data{nullptr, &OptionStorage::nullDeleter}
    {
    }

    template<typename T> 
    T& get()
    {
        if(!_data)
        {
            static auto deleter = [](const void* data)
            {
                delete static_cast<const T*>(data);
            };
            _data = Data(new T{}, deleter);

            static auto cloner = [](OptionStorage& b)
            {
                OptionStorage clone;
                clone.get<T>() = b.get<T>();
                return clone;
            };
            _cloner = cloner;

            static auto combiner = [](OptionStorage& a, OptionStorage& b)
            {
                combineValues(a.get<T>(), b.get<T>());
            };
            _combiner = combiner;

            static auto deduplicator = [](OptionStorage& a)
            {
                deduplicateValues(a.get<T>());
            };
            _deduplicator = deduplicator;
        }
        return *static_cast<T*>(_data.get());
    }

    void combine(OptionStorage& other)
    {
        _combiner(*this, other);
    }

    void deduplicate()
    {
        _deduplicator(*this);
    }

    OptionStorage clone()
    {
        return _cloner(*this);
    }

private:
    template<typename U>
    static void combineValues(U& a, U b)
    {
        a = b;
    }

    template<typename U>
    static void combineValues(std::vector<U>& a, std::vector<U> b)
    {
        a.insert(a.end(), std::make_move_iterator(b.begin()), std::make_move_iterator(b.end()));
    }

    template<typename U>
    static void deduplicateValues(U& v)
    {
    }

    template<typename U>
    static void deduplicateValues(std::vector<U>& v)
    {
        // Tested a few methods and this was the fastest one I came up with that's also pretty simple

        // Could probably also use a custom insertion ordered set instead of vectors to hold options
        // from the start, but this was simpler (and some quick tests indicated possibly faster)
 
        struct DerefEqual
        {
            bool operator ()(const U* a, const U* b) const
            {
                return *a == *b;
            }
        };

        struct DerefHash
        {
            size_t operator ()(const U* a) const
            {
                return OptionHash<U>()(*a);
            }
        };

        std::unordered_set<const U*, DerefHash, DerefEqual> dups;
        dups.reserve(v.size());
        v.erase(std::remove_if(v.begin(), v.end(), [&dups](const U& a) { 
            return !dups.insert(&a).second;
        }), v.end());
    }

    static void nullDeleter(const void*) {}

    std::function<OptionStorage(OptionStorage&)> _cloner;
    std::function<void(OptionStorage&, OptionStorage&)> _combiner;
    std::function<void(OptionStorage&)> _deduplicator;
    Data _data;
};

struct OptionCollection
{
    template<typename T>
    T& operator[](Option<T> option)
    {
        return _storage[option.id].template get<T>();
    }

    void combine(OptionCollection& other)
    {
        for(auto& entry : other._storage)
        {
            auto it = _storage.find(entry.first);
            if(it != _storage.end())
            {
                it->second.combine(entry.second);
            }
            else
            {
                _storage[entry.first] = entry.second.clone();
            }
        }
    }

    void deduplicate()
    {
        for(auto& entry : _storage)
        {
            entry.second.deduplicate();
        }
    }

private:
    std::map<const char*, OptionStorage> _storage;
};

struct ProjectConfig
{
    OptionCollection options;
    std::vector<Project*> links;

    template<typename T>
    T& operator[](Option<T> option)
    {
        return options[option];
    }
};

struct Project : public ProjectConfig
{
    std::string name;
    std::optional<ProjectType> type;
    std::map<ConfigSelector, ProjectConfig, std::less<>> configs;

    Project(std::string name = {}, std::optional<ProjectType> type = {})
        : name(std::move(name)), type(type)
    {
    }

    ProjectConfig resolve(std::optional<ProjectType> projectType, std::string_view configName)
    {
        auto config = internalResolve(projectType, configName, true);
        config.options.deduplicate();
        return config;
    }

    ProjectConfig& operator[](ConfigSelector selector)
    {
        return configs[selector];
    }
    
    template<typename T>
    T& operator[](Option<T> option)
    {
        return options[option];
    }

    void discover(std::set<Project*>& discoveredProjects, std::vector<Project*>& orderedProjects)
    {
        for(auto& link : links)
        {
            link->discover(discoveredProjects, orderedProjects);
        }

        if(discoveredProjects.insert(this).second)
        {
            orderedProjects.push_back(this);
        }
    }

    fs::path calcOutputPath(ProjectConfig& resolvedConfig)
    {
        auto path = resolvedConfig[OutputPath];
        if(!path.empty())
        {
            return path;
        }

        auto stem = resolvedConfig[OutputStem];
        if(stem.empty())
        {
            stem = name;
        }

        return resolvedConfig[OutputDir] / (resolvedConfig[OutputPrefix] + stem + resolvedConfig[OutputSuffix] + resolvedConfig[OutputStem]);
    }

private:
    ProjectConfig internalResolve(std::optional<ProjectType> projectType, std::string_view configName, bool local)
    {
        std::vector<ProjectConfig*> resolveConfigs;

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
            resolveConfigs.push_back(&entry.second);
        }

        ProjectConfig result;

        auto resolveLink = [&](Project* link)
        {
            auto resolved = link->internalResolve(projectType, configName, false);
            result.links += resolved.links;
            result.options.combine(resolved.options);
        };

        for(auto& link : links)
        {
            resolveLink(link);
        }

        for(auto config : resolveConfigs)
        {
            for(auto& link : config->links)
            {
                resolveLink(link);
            }
        }

        auto addOptions = [](auto& a, auto& b)
        {
            a.combine(b);
        };

        if(local)
        {
            addOptions(result.options, options);
        }
        for(auto config : resolveConfigs)
        {
            addOptions(result.options, config->options);
        }
        
        return result;
    }
};

class NinjaEmitter
{
public:
    static void emit(fs::path targetPath, std::set<Project*> projects, const std::string& config = {})
    {
        fs::create_directories(targetPath);

        auto outputFile = targetPath / "build.ninja";
        NinjaEmitter ninja(outputFile);

        // TODO: Proper toolchain config
        ninja.rule("cpp", "clang++ $flags -MMD -MF ${out}.d -c -o $out $in", "${out}.d", "gcc");
        ninja.rule("link", "clang++ $flags -o $out $in");
        ninja.rule("lib", "ar -rcs $flags $out $in");
        ninja.rule("copy", "cp $in $out");

        std::vector<Project*> orderedProjects;
        std::set<Project*> discoveredProjects;
        for(auto project : projects)
        {
            project->discover(discoveredProjects, orderedProjects);
        }

        std::vector<fs::path> generatorDependencies;
        for(auto& project : orderedProjects)
        {
            for(auto& file : (*project)[Files])
            {
                if(fs::is_directory(file))
                {
                    generatorDependencies.push_back(file);
                }
            }
        }

        auto buildOutput = fs::path(BUILD_FILE).replace_extension("");
        Project build("_build", Executable);
        build[Features] += { "c++17", "optimize" };
        build[IncludePaths] += BUILD_H_DIR;
        build[OutputPath] = buildOutput;
        build[Defines] += {
            "START_DIR=\\\"" START_DIR "\\\"",
            "BUILD_H_DIR=\\\"" BUILD_H_DIR "\\\"",
            "BUILD_DIR=\\\"" BUILD_DIR "\\\"",
            "BUILD_FILE=\\\"" BUILD_FILE "\\\"",
            "BUILD_ARGS=\\\"" BUILD_ARGS "\\\"",
        };
        build[Files] += BUILD_FILE;
        orderedProjects.push_back(&build);

        Project generate("_generate", Command);
        generate.links += &build;
        generate[Dependencies] += generatorDependencies;
        generate[Dependencies] += buildOutput;
        generate[Commands] += { "\"" + (BUILD_DIR / buildOutput).string() + "\" " BUILD_ARGS, { outputFile }, START_DIR, true };
        orderedProjects.push_back(&generate);

        for(auto project : orderedProjects)
        {
            auto outputName = emitProject(targetPath, *project, config);
            if(!outputName.empty())
            {
                ninja.subninja(outputName);
            }
        }
    }

private:
    static fs::path fixPath(fs::path path, fs::path root)
    {
        if(path.is_absolute())
        {
            return path;
        }

        return fs::proximate(path, root);
    }
    
    static std::string getCppFlags(const fs::path& root, OptionCollection& options)
    {
        std::string flags;

        for(auto& define : options[Defines])
        {
            flags += " -D\"" + define + "\"";
        }
        for(auto& path : options[IncludePaths])
        {
            if(fs::path(path).is_absolute())
            {
                flags += " -I\"" + path.string() + "\"";
            }
            else
            {
                flags += " -I\"" + fixPath(path, root).string() + "\"";
            }
        }
        if(options[Platform] == "x64")
        {
            flags += " -m64 -arch x86_64";
        }
        for(auto& feature : options[Features])
        {
            // TODO: These should probably be identifiers and not just literals
            if(feature == "c++17")
            {
                flags += " -std=c++17";
            }
            else if(feature == "libc++")
            {
                flags += " -stdlib=libc++";
            }
            else if(feature == "optimize")
            {
                flags += " -O3";
            }
            else if(feature == "debuginfo")
            {
                flags += " -g";
            }
        }

        for(auto& flag : options[ClangCppFlags])
        {
            flags += " " + flag;
        }

        return flags;
    }

    static std::string getLibFlags(const fs::path& root, OptionCollection& options)
    {
        return {};
    }

    static std::string getSharedLinkFlags(const fs::path& root, OptionCollection& options)
    {
        std::string flags;

        for(auto& lib : options[Libs])
        {
            flags += " " + fixPath(lib, root).string();
        }

        for(auto& flag : options[ClangLinkFlags])
        {
            flags += " " + flag;
        }

        for(auto& framework : options[Frameworks])
        {
            flags += " -framework " + framework;
        }

        auto features = options[Features];
        if(std::find(features.begin(), features.end(), "bundle") != features.end())
        {
            flags += " -bundle";
        }
        else
        {
            flags += " -shared";
        }

        return flags;
    }

    static std::string getAppLinkFlags(const fs::path& root, OptionCollection& options)
    {
        std::string flags;

        for(auto& lib : options[Libs])
        {
            flags += " " + fixPath(lib, root).string();
        }

        for(auto& flag : options[ClangLinkFlags])
        {
            flags += " " + flag;
        }

        for(auto& framework : options[Frameworks])
        {
            flags += " -framework " + framework;
        }
        
        return flags;
    }

    static std::string emitProject(fs::path& root, Project& project, std::string_view config)
    {
        static const Option<std::vector<fs::path>> LinkedOutputs{"_LinkedOutputs"};

        auto resolved = project.resolve(project.type, config);

        for(auto& processor : resolved[PostProcess])
        {
            processor(project, resolved);
        }

        if(!project.type.has_value())
        {
            return {};
        }

        if(project.name.empty())
        {
            throw std::runtime_error("Trying to emit project with no name.");
        }

        std::cout << "Emitting '" << project.name << "'";
        if(!config.empty())
        {
            std::cout << " (" << config << ")";
        }
        std::cout << "\n";

        auto ninjaName = project.name + ".ninja";
        NinjaEmitter ninja(root / ninjaName);

        fs::path binDir = "bin";

        std::string cppFlags = getCppFlags(root, resolved.options);

        std::string mmFlags = cppFlags;

        std::vector<std::string> implicitInputs;

        for(auto& dependency : resolved[Dependencies])
        {
            if(fs::is_directory(dependency))
            {
                ninja.build({ fixPath(dependency, root) }, "phony", {});
            }
            implicitInputs.push_back(fixPath(dependency, root).string());
        }

        auto buildPch = resolved[BuildPch];
        auto importPch = resolved[ImportPch];

        std::vector<fs::path> outputs;

        if(!buildPch.empty())
        {
            std::string input = fixPath(buildPch, root);
            std::string pch = fixPath(root / "pch" / buildPch, root).string() + ".pch";

            ninja.build({ pch }, "cpp", { input }, {}, {}, {{"flags", "-x c++-header -Xclang -emit-pch " + cppFlags}});
            if(!windows)
            {
                ninja.build({pch + "mm"}, "cpp", { input }, {}, {}, {{"flags", "-x objective-c++-header -Xclang -emit-pch " + mmFlags}});
            }
        }

        if(!importPch.empty())
        {
            std::string pch = fixPath(root / "pch" / importPch, root).string() + ".pch";
            cppFlags += " -Xclang -include-pch -Xclang " + pch;
            mmFlags += " -Xclang -include-pch -Xclang " + pch + "mm";
            implicitInputs.push_back(pch);
            implicitInputs.push_back(pch + "mm");
        }
        
        ninja.variable("cpp_flags", cppFlags);
        ninja.variable("mm_flags", mmFlags);

        std::vector<std::string> objs;
        for(auto& file : resolved[Files])
        {
            auto ext = fs::path(file).extension().string();
            auto exts = { ".c", ".cpp", ".mm" }; // TODO: Not hardcode these maybe
            if(std::find(exts.begin(), exts.end(), ext) == exts.end()) continue;

            auto obj = (fs::path("obj") / project.name / file).string() + ".o";
            ninja.build({ obj }, "cpp", { fixPath(file, root) }, implicitInputs, {}, {{"flags", ext == ".mm" ? "$mm_flags" : "$cpp_flags"}});
            objs.push_back(obj);
        }

        if(project.type == StaticLib)
        {
            if(objs.empty())
            {
                throw std::runtime_error("Static Library project '" + project.name + "' has no input files.");
            }

            std::string libFlags = getLibFlags(root, resolved.options);
            auto outputFile = project.calcOutputPath(resolved);
            outputs.push_back(outputFile);
            ninja.build({ fixPath(outputFile, root).string() }, "lib", objs, {}, {}, {{"flags", libFlags}});

            project[Public / config][LinkedOutputs] += outputs;
        }
        else if(project.type == SharedLib)
        {
            for(auto& output : resolved[LinkedOutputs])
            {
                objs.push_back(fixPath(output, root));
            }

            if(objs.empty())
            {
                throw std::runtime_error("Shared Library project '" + project.name + "' has no input files or libraries.");
            }

            std::string linkFlags = getSharedLinkFlags(root, resolved.options);
            auto outputFile = project.calcOutputPath(resolved);
            outputs.push_back(outputFile);
            ninja.build({ fixPath(outputFile, root).string() }, "link", objs, {}, {}, {{"flags", linkFlags}});
            
            // TODO: Actually use this
            project[config][BundleContents] += { fixPath(outputFile, root), fs::path("Contents/MacOS") / project.name };
        }
        else if(project.type == Executable)
        {
            for(auto& output : resolved[LinkedOutputs])
            {
                objs.push_back(fixPath(output, root));
            }

            if(objs.empty())
            {
                throw std::runtime_error("Executable project '" + project.name + "' has no input files or libraries.");
            }

            std::string linkFlags = getAppLinkFlags(root, resolved.options);
            auto outputFile = project.calcOutputPath(resolved);
            outputs.push_back(outputFile);
            ninja.build({ fixPath(outputFile, root).string() }, "link", objs, {}, {}, {{"flags", linkFlags}});

            // TODO: Actually use this
            project[config][BundleContents] += { fixPath(outputFile, root), fs::path("Contents/MacOS") / project.name };
        }
        else if(project.type == Command)
        {
            auto& commands = resolved[Commands];
            if(commands.empty())
            {
                throw std::runtime_error("Command project '" + project.name + "' has no commands.");
            }

            int index = 1;
            for(auto& command : commands)
            {
                auto ruleName = "command_" + std::to_string(index++);
                std::string prologue;
                if(windows)
                {
                    prologue += "cmd /c ";
                }
                fs::path cwd = command.workingDirectory;
                if(cwd.empty())
                {
                    cwd = ".";
                }
                prologue += "cd \"" + fixPath(cwd, root).string() + "\" && ";
                ninja.rule(ruleName, prologue + command.command);
                std::vector<std::string> strOutputs;
                strOutputs.reserve(command.outputs.size());
                for(auto& path : command.outputs)
                {
                    strOutputs.push_back(fixPath(path, root).string());
                }

                std::vector<std::pair<std::string_view, std::string_view>> variables;
                if(command.generator)
                {
                    variables.push_back({"generator", "1"});
                }
                ninja.build(strOutputs, ruleName, {}, implicitInputs, {}, variables);
            }
        }

        std::vector<std::string> offsetOutputs;
        for(auto& outputPath : outputs)
        {
            offsetOutputs += fixPath(outputPath, root).string();
        }

        ninja.build({ project.name }, "phony", offsetOutputs);

        return ninjaName;
    }

private:
    NinjaEmitter(fs::path path)
        : _stream(path)
    {
    }

    std::ofstream _stream;

    void subninja(std::string_view name)
    {
        _stream << "subninja " << name << "\n";
    }

    void variable(std::string_view name, std::string_view value)
    {
        _stream << name << " = " << value << "\n";
    }

    void rule(std::string_view name, std::string_view command, std::string_view depfile = {}, std::string_view deps = {})
    {
        _stream << "rule " << name << "\n";
        _stream << "  command = " << command << "\n";
        if(!depfile.empty())
        {
            _stream << "  depfile = " << depfile << "\n";
        }
        if(!deps.empty())
        {
            _stream << "  deps = " << deps << "\n";
        }
        _stream << "\n";
    }

    void build(const std::vector<std::string>& outputs, std::string_view rule, const std::vector<std::string>& inputs, const std::vector<std::string>& implicitInputs = {}, const std::vector<std::string>& orderInputs = {}, std::vector<std::pair<std::string_view, std::string_view>> variables = {})
    {
        _stream << "build ";
        for(auto& output : outputs)
        {
            _stream << output << " ";
        }

        _stream << ": " << rule << " ";

        for(auto& input : inputs)
        {
            _stream << input << " ";
        }

        if(!implicitInputs.empty())
        {
            _stream << "| ";
            for(auto& implicitInput : implicitInputs)
            {
                _stream << implicitInput << " ";
            }
        }
        if(!orderInputs.empty())
        {
            _stream << "|| ";
            for(auto& orderInput : orderInputs)
            {
                _stream << orderInput << " ";
            }
        }
        _stream << "\n";
        for(auto& variable : variables)
        {
            _stream << "  " << variable.first << " = " << variable.second << "\n";
        }

        _stream << "\n";
    }
};

struct RunResult
{
    int exitCode;
    std::string output;
};

RunResult runCommand(std::string command)
{
    RunResult result;
    {
        auto processPipe = popen(command.c_str(), "r");
        try
        {
            std::array<char, 2048> buffer;
            while(auto bytesRead = fread(buffer.data(), 1, buffer.size(), processPipe))
            {
                result.output.append(buffer.data(), bytesRead);
            }
            
        }
        catch(...)
        {
            pclose(processPipe);
            throw;
        }
        auto status = pclose(processPipe);
        result.exitCode = WEXITSTATUS(status);
    }

    return result;
}

std::string readFile(fs::path path)
{
    std::ifstream stream(path);
    std::stringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

struct JsonValue
{
    enum Type
    {
        VALUE,
        OBJECT,
        ARRAY,
        ERROR
    } type;

    JsonValue() : error("No value"), type(ERROR) {}
    JsonValue(std::string_view value) : value(value), type(VALUE) {}
    JsonValue(std::map<std::string_view, JsonValue> object) : object(object), type(OBJECT) {}
    JsonValue(std::vector<JsonValue> array) : array(array), type(ARRAY) {}
    std::string_view value;
    std::map<std::string_view, JsonValue> object;
    std::vector<JsonValue> array;
    std::string error;

    static JsonValue errorValue(std::string message)
    {
        JsonValue value;
        value.error = std::move(message);
        value.type = ERROR;
        return value;
    }

    size_t size() const
    {
        switch(type)
        {
        case ERROR:
        case VALUE:
            return 0;
        case OBJECT:
            return object.size();
        case ARRAY:
            return array.size();
        }
    }

    bool empty() const
    {
        switch(type)
        {
        case ERROR:
            return true;
        case VALUE:
            return value.empty();
        case OBJECT:
            return object.empty();
        case ARRAY:
            return array.empty();
        }
    }

    const JsonValue& operator [](int index) const
    {
        if(type == ERROR)
        {
            return *this;
        }
        else if(type != ARRAY)
        {
            static JsonValue value = errorValue("Entry is not an array.");
            return value;
        }
        else if(index < 0 || index >= array.size())
        {
            static JsonValue value = errorValue("Index out of range.");
            return value;
        }
        return array[index];
    }

    const JsonValue& operator [](std::string_view key) const 
    {
        if(type == ERROR)
        {
            return *this;
        }
        else if(type != OBJECT)
        {
            static JsonValue value = errorValue("Entry is not an object.");
            return value;
        }
        
        auto it = object.find(key);
        if(it != object.end())
        {
            return it->second;
        }

        {
            static JsonValue value = errorValue("Key not found.");
            return value;
        }
    }

    operator const std::string_view& () const
    {
        return value;
    }

    std::string_view unquoted() const
    {
        if(value.size() >= 2 && value[0] == '"' && value[value.size()-1] == '"')
        {
            return std::string_view(value.begin()+1, value.size() - 2);
        }
        return value;
    }

private:
};

// Believe it or not but this is not a fantastic JSON parser
JsonValue parseJson(std::string_view::iterator& it, std::string_view::iterator end)
{
    auto skipWhitespace = [&]()
    {
        while(std::isspace(*it))
        {
            ++it;
            if(it == end)
            {
                return false;
            }
        }
        return true;
    };

    auto readUntilSeparator = [&](){
        if(!skipWhitespace())
        {
            return JsonValue::errorValue("Unexpected end of file.");
        }

        auto start = it;
        bool inString = false;
        char prev = 0;
        while(inString || (*it != ',' && *it != '}' && *it != ']' && *it != ':')) 
        {
            if(*it == '"' && prev != '\\')
            {
                inString = !inString;
            }
            prev = *it;
            ++it;
            if(it == end)
            {
                return JsonValue::errorValue("Unexpected end of file.");
            }
        }
        return JsonValue(std::string_view(start, it-start));
    };

    if(!skipWhitespace())
    {
        return JsonValue::errorValue("Unexpected end of file.");
    }
    if(*it == '[')
    {
        ++it;

        std::vector<JsonValue> array;
        while(true)
        {
            auto value = readUntilSeparator();
            if(value.type == JsonValue::ERROR)
            {
                return value;
            }

            array.push_back(std::move(value));
            
            if(*it == ']')
            {
                ++it;
                return JsonValue(std::move(array));
            }
            else if(*it != ',')
            {
                return JsonValue::errorValue(std::string("Unexpected '") + *it + "'.");
            }
            ++it;
        }
    }
    else if(*it == '{')
    {
        ++it;

        std::map<std::string_view, JsonValue> object;
        while(true)
        {
            auto key = readUntilSeparator();
            if(key.type == JsonValue::ERROR)
            {
                return key;
            }
            key.value = key.unquoted();

            if(*it != ':')
            {
                return JsonValue::errorValue(std::string("Unexpected '") + *it + "' after reading object key '" + std::string(key.value) + "'.");
            }
            ++it;

            auto value = parseJson(it, end);
            if(value.type == JsonValue::ERROR)
            {
                return value;
            }
            object.insert(std::make_pair(key.value, std::move(value)));

            if(*it == '}')
            {
                ++it;
                return JsonValue(std::move(object));
            }
            else if(*it != ',')
            {
                return JsonValue::errorValue(std::string("Unexpected '") + *it + "' after reading object value with key '" + std::string(key.value) + "'.");
            }
            ++it;
        }
    }
    else
    {
        return readUntilSeparator();
    }
}

JsonValue parseJson(std::string_view json)
{
    auto it = json.begin();
    return parseJson(it, json.end());
}

std::pair<std::string, std::string> splitString(std::string_view str, char delimiter)
{
    auto pos = str.find(delimiter);
    if(pos != str.npos)
    {
        return { std::string(str.substr(0, pos)), std::string(str.substr(pos+1, str.size()-pos-1)) };
    }
    else
    {
        return { std::string(str), "" };
    }
}

std::vector<std::pair<std::string, std::string>> parseOptionArguments(const std::vector<std::string> arguments)
{
    std::vector<std::pair<std::string, std::string>> result;
    for(auto& arg : arguments)
    {
        if(arg.size() > 1 && arg[0] == '-' && arg[1] == '-')
        {
            result.push_back(splitString(arg.substr(2), '='));
        }
    }

    return result;
}

std::vector<std::string> parsePositionalArguments(const std::vector<std::string> arguments, bool skipFirst = true)
{
    std::vector<std::string> result;
    for(auto& arg : arguments)
    {
        if(skipFirst)
        {
            skipFirst = false;
            continue;
        }
        if(arg.size() < 2 || arg[0] != '-' || arg[1] != '-')
        {
            result.push_back(arg);
        }
    }

    return result;
}

std::vector<fs::path> sourceList(fs::path path, bool recurse = true)
{
    std::vector<fs::path> result;
    if(!fs::exists(path) || !fs::is_directory(path))
    {
        throw std::runtime_error("Source directory '" + path.string() + "' does not exist.");
        return {};
    }

    // Add the directory as a dependency to rescan if the contents change
    result.push_back(path);

    for(auto entry : fs::recursive_directory_iterator(path))
    {
        if(entry.is_directory())
        {
            // Add subdirectories as dependencies to rescan if the contents change
            result.push_back(path);
            continue;
        }
        if(!entry.is_regular_file()) continue;

        auto exts = { ".c", ".cpp", ".mm", ".h", ".hpp" }; // TODO: Not hardcode these maybe
        auto ext = entry.path().extension().string();
        if(std::find(exts.begin(), exts.end(), ext) != exts.end())
        {
            result += entry.path();
        }
    }

    return result;
}

void parseCommandLineAndEmit(fs::path startPath, const std::vector<std::string> arguments, std::set<Project*> projects, std::set<std::string> configs)
{
    auto optionArgs = parseOptionArguments(arguments);
    auto positionalArgs = parsePositionalArguments(arguments);

    if(configs.empty())
    {
        throw std::runtime_error("No configurations available.");
    }

    std::vector<std::string> availableEmitters = { "ninja" };
    std::vector<std::pair<std::string, fs::path>> emitters;
    for(auto& arg : optionArgs)
    {
        if(std::find(availableEmitters.begin(), availableEmitters.end(), arg.first) != availableEmitters.end())
        {
            auto targetDir = arg.second;
            if(targetDir.empty())
            {
                targetDir = arg.first + "build";
            }
            emitters.push_back({arg.first, targetDir});
        }
    }

    if(emitters.empty())
    {
        std::cout << "Usage: " << arguments[0] << " --emitter[=†argetDir]\n";
        std::cout << "Example: " << arguments[0] << " --ninja=ninjabuild\n\n";
        std::cout << "Available emitters: \n";
        for(auto& emitter : availableEmitters)
        {
            std::cout << "  --" << emitter << "\n";
        }
        std::cout << "\n\n";
        throw std::runtime_error("No emitters specified.");
    }

    for(auto& emitter : emitters)
    {
        if(emitter.first == "ninja")
        {
            for(auto& config : configs)
            {
                auto outputPath = emitter.second / config;
                if(!outputPath.is_absolute())
                {
                    outputPath = startPath / outputPath;
                }
                NinjaEmitter::emit(outputPath, projects, config);
            }
        }
    }
}

void generate(fs::path startPath, std::vector<std::string> args);
int main(int argc, const char** argv)
{
    try
    {
        auto startPath = fs::current_path();
        fs::current_path(BUILD_DIR);
        startPath = fs::proximate(startPath);
        generate(startPath, std::vector<std::string>(argv, argv+argc));
    }
    catch(const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return -1;
    }
}