#ifndef FUNNYSEMA_SEMA_SNAPSHOT_H
#define FUNNYSEMA_SEMA_SNAPSHOT_H

#include <filesystem>
#include <vector>

#include "Snapshot.h"

namespace funny {

std::vector<SnapshotResult> run_sema_snapshots(const std::filesystem::path& tests_dir, bool bless);

}  // namespace funny

#endif  // FUNNYSEMA_SEMA_SNAPSHOT_H
