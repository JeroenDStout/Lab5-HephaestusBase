/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Assert.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Stringstream.h"
#include "BlackRoot/Pubc/Sys Thread.h"

#include "HephaestusBase/Pubc/Pipe Wrangler.h"

namespace sys = BlackRoot::System;
using namespace Hephaestus::Pipeline;
using namespace Hephaestus::Pipeline::Wrangler;

    //  Setup
    // --------------------

PipeWrangler::PipeWrangler()
: Caller([&](){this->ThreadedCall();})
{
    this->MaxThreadCount    = std::thread::hardware_concurrency();
}

PipeWrangler::~PipeWrangler()
{
}

    //  Cycle
    // --------------------

void PipeWrangler::ThreadedCall()
{
    using cout = BlackRoot::Util::Cout;

    std::unique_lock<std::mutex> lk(this->MxTasks);

    if (this->Tasks.size() == 0)
        return;
    
    auto task = std::move(this->Tasks.front());
    this->Tasks.erase(this->Tasks.begin());
    lk.unlock();

    WranglerTaskResult result;
    result.Exception = nullptr;
    result.UniqueID = task.OriginTask->UniqueID;
    
    Pipeline::PipeToolInstr instr;

    try {
        auto tool = this->FindTool(task.OriginTask->ToolName);

        instr.FileIn    = task.OriginTask->FileIn;
        instr.FileOut   = task.OriginTask->FileOut;
        instr.Settings  = std::move(task.OriginTask->Settings);

        auto startTime = std::chrono::system_clock::now();
        tool->Run(instr);
        result.ProcessDuration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime);
    }
    catch (BlackRoot::Debug::Exception * e) {
        result.Exception = e;
    }
    catch (std::exception e) {
        result.Exception = new BlackRoot::Debug::Exception(e.what(), {});
    }
    catch (...) {
        result.Exception = new BlackRoot::Debug::Exception("Unknown error trying to process task", {});
    }

    if (result.Exception) {
        cout{} << std::endl << "Pipe error: " << task.OriginTask->ToolName << std::endl
            << " " << task.OriginTask->FileIn << std::endl
            << " " << task.OriginTask->FileOut << std::endl
            << " " << task.OriginTask->Settings.dump() << std::endl
            << " " << result.Exception->GetPrettyDescription() << std::endl;
    }

    for (auto & it : instr.ReadFiles) {
        result.ReadFiles.push_back({ it.Path, it.LastChange });
    }
    for (auto & it : instr.WrittenFiles) {
        result.WrittenFiles.push_back({ it.Path });
    }

    task.OriginTask->Callback(result);
    delete task.OriginTask;
}

    //  Control
    // --------------------

void PipeWrangler::Begin()
{
    this->Caller.SetMaxThreadCount(this->MaxThreadCount);
}

void PipeWrangler::EndAndWait()
{
    this->Caller.EndAndWait();
}

    //  Tools
    // --------------------

void PipeWrangler::RegisterTool(const DynLib::IPipeTool * tool)
{
    std::unique_lock<std::shared_mutex> lk(this->MxTools);
    this->Tools[tool->GetToolName()] = tool;
}

const DynLib::IPipeTool * PipeWrangler::FindTool(std::string str)
{
    std::shared_lock<std::shared_mutex> lk(this->MxTools);

    auto & it = this->Tools.find(str);
    if (it == this->Tools.end()) {
        throw new BlackRoot::Debug::Exception((std::stringstream() << "The tool '" << str << "' is not known.").str(), BRGenDbgInfo);
    }
    return it->second;
}

    //  Asynch
    // --------------------
        
void PipeWrangler::AsynchReceiveTasks(const WranglerTaskList &list)
{
    std::vector<Task> newTasks;

    for (auto & inTask : list) {
        Task task;
        task.OriginTask = new WranglerTask(inTask);
        newTasks.push_back(std::move(task));
    }

    std::unique_lock<std::mutex> lk(this->MxTasks);
    this->Tasks.insert(this->Tasks.end(), std::make_move_iterator(newTasks.begin()), 
                                          std::make_move_iterator(newTasks.end()));

    // TODO: sort

    lk.unlock();

    this->Caller.RequestCalls(newTasks.size());
}
    //  Util
    // --------------------

std::string PipeWrangler::GetAvailableTools()
{
    std::stringstream ss;

    std::shared_lock<std::shared_mutex> lk(this->MxTools);
    bool first = true;
    for (auto & it : this->Tools) {
        if (!first) ss << ", ";
        ss << it.first;
        first = false;
    }
    lk.unlock();

    return ss.str();
}