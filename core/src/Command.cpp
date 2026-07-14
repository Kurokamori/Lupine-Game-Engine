#include "lupine/core/Command.hpp"
#include "lupine/core/EditorCommands.hpp"  // CreateCommandForType
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace core {

CompositeCommand::CompositeCommand(const std::string& description)
    : m_Description(description), m_Executed(false) {
}

void CompositeCommand::AddCommand(std::shared_ptr<Command> command) {
    if (command) {
        m_Commands.push_back(command);
    }
}

bool CompositeCommand::Execute() {
    if (m_Executed) {

        return Redo();
    }

    for (auto& command : m_Commands) {
        if (!command->Execute()) {

            for (auto it = m_Commands.rbegin(); it != m_Commands.rend(); ++it) {
                if (it->get() == command.get()) {
                    break;
                }
                (*it)->Undo();
            }

            return false;
        }
    }

    m_Executed = true;

    return true;
}

bool CompositeCommand::Undo() {

    for (auto it = m_Commands.rbegin(); it != m_Commands.rend(); ++it) {
        if (!(*it)->Undo()) {

            return false;
        }
    }

    return true;
}

bool CompositeCommand::Redo() {

    for (auto& command : m_Commands) {
        if (!command->Redo()) {

            return false;
        }
    }

    return true;
}

std::string CompositeCommand::GetDescription() const {
    return m_Description;
}

nlohmann::json CompositeCommand::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["description"] = m_Description;
    json["executed"] = m_Executed;

    nlohmann::json commandsArray = nlohmann::json::array();
    for (const auto& command : m_Commands) {
        commandsArray.push_back(command->Serialize());
    }
    json["commands"] = commandsArray;

    return json;
}

bool CompositeCommand::Deserialize(const nlohmann::json& json) {
    m_Description = json.value("description", std::string());
    m_Executed = json.value("executed", false);
    m_Commands.clear();

    if (!json.contains("commands") || !json["commands"].is_array()) {
        return false;
    }
    for (const auto& cmdJson : json["commands"]) {
        const std::string type = cmdJson.value("type", std::string());
        std::shared_ptr<Command> cmd = CreateCommandForType(type);
        if (!cmd || !cmd->Deserialize(cmdJson)) {
            LOG_WARN(LogCategory::Tools, "CompositeCommand: skipping sub-command of unknown/invalid type '{}'", type);
            continue;
        }
        m_Commands.push_back(cmd);
    }
    return true;
}

CommandHistory::CommandHistory()
    : m_CurrentIndex(0), m_MaxHistorySize(100) {
}

CommandHistory::CommandHistory(size_t maxHistorySize)
    : m_CurrentIndex(0), m_MaxHistorySize(maxHistorySize) {
}

bool CommandHistory::ExecuteCommand(std::shared_ptr<Command> command) {
    if (!command) {

        return false;
    }

    if (!command->Execute()) {

        return false;
    }

    RecordCommand(command);
    return true;
}

void CommandHistory::RecordCommand(std::shared_ptr<Command> command) {
    if (!command) {

        return;
    }

    if (m_CurrentIndex > 0 && m_CurrentIndex <= m_Commands.size()) {
        auto& prevCommand = m_Commands[m_CurrentIndex - 1];
        if (prevCommand->CanMergeWith(command.get())) {

            prevCommand->MergeWith(command.get());
            return;
        }
    }

    if (m_CurrentIndex < m_Commands.size()) {

        m_Commands.erase(m_Commands.begin() + m_CurrentIndex, m_Commands.end());
    }

    m_Commands.push_back(command);
    m_CurrentIndex++;

    TrimHistory();

}

bool CommandHistory::Undo() {
    if (!CanUndo()) {

        return false;
    }

    auto& command = m_Commands[m_CurrentIndex - 1];

    if (!command->Undo()) {

        return false;
    }

    m_CurrentIndex--;

    return true;
}

bool CommandHistory::Redo() {
    if (!CanRedo()) {

        return false;
    }

    auto& command = m_Commands[m_CurrentIndex];

    if (!command->Redo()) {

        return false;
    }

    m_CurrentIndex++;

    return true;
}

bool CommandHistory::CanUndo() const {
    return m_CurrentIndex > 0;
}

bool CommandHistory::CanRedo() const {
    return m_CurrentIndex < m_Commands.size();
}

void CommandHistory::Clear() {
    m_Commands.clear();
    m_CurrentIndex = 0;

}

void CommandHistory::SetMaxHistorySize(size_t size) {
    m_MaxHistorySize = size;
    TrimHistory();
}

size_t CommandHistory::GetRedoStackSize() const {
    return m_Commands.size() - m_CurrentIndex;
}

std::string CommandHistory::GetUndoDescription() const {
    if (!CanUndo()) {
        return "";
    }
    return m_Commands[m_CurrentIndex - 1]->GetDescription();
}

std::string CommandHistory::GetRedoDescription() const {
    if (!CanRedo()) {
        return "";
    }
    return m_Commands[m_CurrentIndex]->GetDescription();
}

nlohmann::json CommandHistory::Serialize() const {
    nlohmann::json json;
    json["max_history_size"] = m_MaxHistorySize;
    json["current_index"] = m_CurrentIndex;

    nlohmann::json commandsArray = nlohmann::json::array();
    for (const auto& command : m_Commands) {
        commandsArray.push_back(command->Serialize());
    }
    json["commands"] = commandsArray;

    return json;
}

bool CommandHistory::Deserialize(const nlohmann::json& json) {
    if (!json.contains("commands") || !json["commands"].is_array()) {
        return false;
    }

    m_MaxHistorySize = json.value("max_history_size", m_MaxHistorySize);
    m_Commands.clear();

    for (const auto& cmdJson : json["commands"]) {
        const std::string type = cmdJson.value("type", std::string());
        std::shared_ptr<Command> cmd = CreateCommandForType(type);
        if (!cmd || !cmd->Deserialize(cmdJson)) {
            LOG_WARN(LogCategory::Tools, "CommandHistory: skipping command of unknown/invalid type '{}'", type);
            continue;
        }
        m_Commands.push_back(cmd);
    }

    // Clamp the restored cursor to the number of commands that actually deserialized
    // (some may have been skipped) so undo/redo bounds stay valid.
    m_CurrentIndex = json.value("current_index", static_cast<size_t>(0));
    if (m_CurrentIndex > m_Commands.size()) {
        m_CurrentIndex = m_Commands.size();
    }
    return true;
}

void CommandHistory::TrimHistory() {
    if (m_Commands.size() <= m_MaxHistorySize) {
        return;
    }

    size_t toRemove = m_Commands.size() - m_MaxHistorySize;
    m_Commands.erase(m_Commands.begin(), m_Commands.begin() + toRemove);

    if (m_CurrentIndex > toRemove) {
        m_CurrentIndex -= toRemove;
    } else {
        m_CurrentIndex = 0;
    }

}

}
}

