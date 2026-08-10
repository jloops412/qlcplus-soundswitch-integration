#include "emberlights/project_edit_history.hpp"

#include <utility>

namespace emberlights {

void ProjectEditHistory::clear() noexcept {
    undo_.clear();
    redo_.clear();
}

void ProjectEditHistory::record_before_change(const ProjectDocument& project) {
    append_bounded(undo_, project);
    redo_.clear();
}

bool ProjectEditHistory::can_undo() const noexcept {
    return !undo_.empty();
}

bool ProjectEditHistory::can_redo() const noexcept {
    return !redo_.empty();
}

std::size_t ProjectEditHistory::undo_count() const noexcept {
    return undo_.size();
}

std::size_t ProjectEditHistory::redo_count() const noexcept {
    return redo_.size();
}

bool ProjectEditHistory::undo(ProjectDocument& project) {
    if (undo_.empty()) {
        return false;
    }
    append_bounded(redo_, std::move(project));
    project = std::move(undo_.back());
    undo_.pop_back();
    return true;
}

bool ProjectEditHistory::redo(ProjectDocument& project) {
    if (redo_.empty()) {
        return false;
    }
    append_bounded(undo_, std::move(project));
    project = std::move(redo_.back());
    redo_.pop_back();
    return true;
}

void ProjectEditHistory::append_bounded(
    std::vector<ProjectDocument>& history,
    ProjectDocument project) {
    if (history.size() == kMaximumProjectUndoEntries) {
        history.erase(history.begin());
    }
    history.push_back(std::move(project));
}

}  // namespace emberlights
