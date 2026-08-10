#pragma once

#include "emberlights/project.hpp"

#include <cstddef>
#include <vector>

namespace emberlights {

// Studio-only document snapshots. This never participates in the Runner or
// persisted project format, and is intentionally bounded to keep authoring
// mistakes recoverable without unbounded memory growth.
inline constexpr std::size_t kMaximumProjectUndoEntries = 100U;

class ProjectEditHistory {
public:
    void clear() noexcept;
    void record_before_change(const ProjectDocument& project);

    [[nodiscard]] bool can_undo() const noexcept;
    [[nodiscard]] bool can_redo() const noexcept;
    [[nodiscard]] std::size_t undo_count() const noexcept;
    [[nodiscard]] std::size_t redo_count() const noexcept;

    bool undo(ProjectDocument& project);
    bool redo(ProjectDocument& project);

private:
    static void append_bounded(
        std::vector<ProjectDocument>& history,
        ProjectDocument project);

    std::vector<ProjectDocument> undo_;
    std::vector<ProjectDocument> redo_;
};

}  // namespace emberlights
