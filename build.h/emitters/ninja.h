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

class NinjaEmitter : public Emitter
{
public:
    static NinjaEmitter instance;

    NinjaEmitter();

    virtual void emit(Environment& env) override;

private:
    static std::string emitProject(const std::filesystem::path& root, Project& project, StringId config, bool generator);
};
