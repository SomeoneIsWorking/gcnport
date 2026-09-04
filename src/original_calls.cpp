// SPDX-License-Identifier: GPL-2.0-or-later
#include "gcnport/original_calls.h"

#include <map>
#include <mutex>
#include <stdexcept>

namespace gcnport {

namespace {

enum class TicketState : std::uint8_t {
  Pending,
  Claimed,
};

struct TicketRecord {
  HookKey key;
  TicketState state = TicketState::Pending;
};

} // namespace

class OriginalCallCoordinator::Storage {
public:
  mutable std::mutex mutex;
  std::map<std::uint64_t, TicketRecord> tickets;
  std::uint64_t next_ticket = 1;
};

OriginalCallCoordinator::OriginalCallCoordinator(CodeInvalidator &invalidator)
    : invalidator_(invalidator), storage_(std::make_unique<Storage>()) {}

OriginalCallCoordinator::~OriginalCallCoordinator() = default;

OriginalCallTicket OriginalCallCoordinator::begin(const HookKey &key) {
  if (!key.is_valid()) {
    throw std::invalid_argument(
        "original call key requires an authenticated image and nonzero aligned PPC address");
  }

  std::lock_guard lock(storage_->mutex);
  const OriginalCallTicket ticket{storage_->next_ticket++};
  invalidator_.invalidate_instruction(key.address);
  storage_->tickets.emplace(ticket.value, TicketRecord{.key = key});
  return ticket;
}

bool OriginalCallCoordinator::claim_entry(OriginalCallTicket ticket, const HookKey &key) {
  std::lock_guard lock(storage_->mutex);
  const auto found = storage_->tickets.find(ticket.value);
  if (found == storage_->tickets.end() || found->second.key != key ||
      found->second.state != TicketState::Pending) {
    return false;
  }
  found->second.state = TicketState::Claimed;
  return true;
}

void OriginalCallCoordinator::complete(OriginalCallTicket ticket) {
  std::lock_guard lock(storage_->mutex);
  const auto found = storage_->tickets.find(ticket.value);
  if (found == storage_->tickets.end() || found->second.state != TicketState::Claimed) {
    throw std::logic_error("original call ticket was not claimed exactly once");
  }
  invalidator_.invalidate_instruction(found->second.key.address);
  storage_->tickets.erase(found);
}

void OriginalCallCoordinator::cancel(OriginalCallTicket ticket) {
  std::lock_guard lock(storage_->mutex);
  const auto found = storage_->tickets.find(ticket.value);
  if (found == storage_->tickets.end()) {
    return;
  }
  invalidator_.invalidate_instruction(found->second.key.address);
  storage_->tickets.erase(found);
}

std::size_t OriginalCallCoordinator::pending_count() const {
  std::lock_guard lock(storage_->mutex);
  return storage_->tickets.size();
}

} // namespace gcnport
