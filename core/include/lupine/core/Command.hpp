#pragma once

#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "lupine/core/UUID.hpp"

namespace lupine {
namespace core {

/**
 * Base class for all undoable commands
 * Implements the Command pattern for undo/redo functionality
 */
class Command {
public:
    virtual ~Command() = default;

    /**
     * Execute the command
     * @return true if successful, false otherwise
     */
    virtual bool Execute() = 0;

    /**
     * Undo the command (reverse the operation)
     * @return true if successful, false otherwise
     */
    virtual bool Undo() = 0;

    /**
     * Redo the command (re-execute after undo)
     * @return true if successful, false otherwise
     */
    virtual bool Redo() { return Execute(); }

    /**
     * Get a human-readable description of the command
     */
    virtual std::string GetDescription() const = 0;

    /**
     * Get the command type name for serialization
     */
    virtual std::string GetTypeName() const = 0;

    /**
     * Serialize the command to JSON
     */
    virtual nlohmann::json Serialize() const = 0;

    /**
     * Deserialize the command from JSON
     */
    virtual bool Deserialize(const nlohmann::json& json) = 0;

    /**
     * Check if this command can be merged with another command
     * Used for combining similar consecutive commands (e.g., multiple property changes)
     */
    virtual bool CanMergeWith(const Command* other) const { return false; }

    /**
     * Merge this command with another command
     */
    virtual void MergeWith(const Command* other) {}

    /**
     * Get unique ID for this command
     */
    const UUID& GetID() const { return m_ID; }

protected:
    UUID m_ID;
};

/**
 * Composite command that groups multiple commands together
 * Executes/undoes all sub-commands as a single atomic operation
 */
class CompositeCommand : public Command {
public:
    CompositeCommand(const std::string& description);

    /**
     * Add a sub-command to this composite
     * Note: Commands are executed in the order they are added
     */
    void AddCommand(std::shared_ptr<Command> command);

    bool Execute() override;
    bool Undo() override;
    bool Redo() override;
    std::string GetDescription() const override;
    std::string GetTypeName() const override { return "CompositeCommand"; }
    nlohmann::json Serialize() const override;
    bool Deserialize(const nlohmann::json& json) override;

    /**
     * Get the number of sub-commands
     */
    size_t GetCommandCount() const { return m_Commands.size(); }

private:
    std::vector<std::shared_ptr<Command>> m_Commands;
    std::string m_Description;
    bool m_Executed = false;
};

/**
 * Command history manager
 * Manages undo/redo stack with configurable history limit
 */
class CommandHistory {
public:
    CommandHistory();
    explicit CommandHistory(size_t maxHistorySize);

    /**
     * Execute and record a command
     */
    bool ExecuteCommand(std::shared_ptr<Command> command);

    /**
     * Record a command without executing it (for commands that have already been executed)
     */
    void RecordCommand(std::shared_ptr<Command> command);

    /**
     * Undo the last command
     */
    bool Undo();

    /**
     * Redo the last undone command
     */
    bool Redo();

    /**
     * Check if undo is available
     */
    bool CanUndo() const;

    /**
     * Check if redo is available
     */
    bool CanRedo() const;

    /**
     * Clear all history
     */
    void Clear();

    /**
     * Get the maximum history size
     */
    size_t GetMaxHistorySize() const { return m_MaxHistorySize; }

    /**
     * Set the maximum history size
     */
    void SetMaxHistorySize(size_t size);

    /**
     * Get the current undo stack size
     */
    size_t GetUndoStackSize() const { return m_CurrentIndex; }

    /**
     * Get the current redo stack size
     */
    size_t GetRedoStackSize() const;

    /**
     * Get description of the command that would be undone
     */
    std::string GetUndoDescription() const;

    /**
     * Get description of the command that would be redone
     */
    std::string GetRedoDescription() const;

    /**
     * Serialize the entire history to JSON
     */
    nlohmann::json Serialize() const;

    /**
     * Deserialize history from JSON
     */
    bool Deserialize(const nlohmann::json& json);

private:
    void TrimHistory();

    std::vector<std::shared_ptr<Command>> m_Commands;
    size_t m_CurrentIndex;  // Points to the next command to execute
    size_t m_MaxHistorySize;
};

} // namespace core
} // namespace lupine

