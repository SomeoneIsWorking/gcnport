// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <memory>

#include "gcnport/native_hooks.h"

namespace gcnport {

struct OriginalCallTicket {
  std::uint64_t value = 0;

  auto operator<=>(const OriginalCallTicket &) const = default;
};

// Owns one-shot suppression independently of the hook registry. The Dolphin adapter must claim a
// ticket only at the requested target, compile an unlinked/unpublished one-shot block, execute it,
// and complete the ticket before ordinary dispatch resumes. This prevents recursion and other CPUs
// from bypassing the installed hook.
class OriginalCallCoordinator {
public:
  explicit OriginalCallCoordinator(CodeInvalidator &invalidator);
  ~OriginalCallCoordinator();
  OriginalCallCoordinator(const OriginalCallCoordinator &) = delete;
  OriginalCallCoordinator(OriginalCallCoordinator &&) = delete;
  OriginalCallCoordinator &operator=(const OriginalCallCoordinator &) = delete;
  OriginalCallCoordinator &operator=(OriginalCallCoordinator &&) = delete;

  // Ticket lifecycle operations happen at the backend's CPU safe point. The invalidator must not
  // call back into this coordinator while a lifecycle operation is in progress.
  [[nodiscard]] OriginalCallTicket begin(const HookKey &key);
  [[nodiscard]] bool claim_entry(OriginalCallTicket ticket, const HookKey &key);
  void complete(OriginalCallTicket ticket);
  void cancel(OriginalCallTicket ticket);
  [[nodiscard]] std::size_t pending_count() const;

private:
  class Storage;

  CodeInvalidator &invalidator_;
  std::unique_ptr<Storage> storage_;
};

} // namespace gcnport
