// SPDX-License-Identifier: GPL-2.0-or-later
#include "gcnport/native_hooks.h"

#include <algorithm>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <utility>

namespace gcnport {

namespace {

void validate_key(const HookKey &key) {
  if (!key.is_valid()) {
    throw std::invalid_argument(
        "native hook key requires an authenticated image and nonzero aligned PPC address");
  }
}

} // namespace

class NativeHookRegistry::Storage {
public:
  mutable std::shared_mutex mutex;
  std::map<HookKey, NativeHook> hooks;
};

bool ImageIdentity::is_authenticated() const noexcept {
  return std::ranges::any_of(sha256, [](std::uint8_t byte) { return byte != 0; });
}

bool HookKey::is_valid() const noexcept {
  return identity.image.is_authenticated() && address != 0 && address % sizeof(std::uint32_t) == 0;
}

HookResult HookResult::return_to_caller() noexcept {
  return {.action = HookAction::ReturnToCaller, .continuation = 0};
}

HookResult HookResult::continue_at(GuestAddress address) noexcept {
  return {.action = HookAction::ContinueAtAddress, .continuation = address};
}

HookResult HookResult::call_original_once() noexcept {
  return {.action = HookAction::CallOriginalOnce, .continuation = 0};
}

NativeHookRegistry::NativeHookRegistry(CodeInvalidator &invalidator)
    : invalidator_(invalidator), storage_(std::make_unique<Storage>()) {}

NativeHookRegistry::~NativeHookRegistry() = default;

void NativeHookRegistry::install(HookKey key, NativeHook hook) {
  validate_key(key);
  if (!hook) {
    throw std::invalid_argument("native hook callback is empty");
  }

  std::unique_lock lock(storage_->mutex);
  invalidator_.invalidate_instruction(key.address);
  storage_->hooks.insert_or_assign(key, std::move(hook));
}

bool NativeHookRegistry::remove(const HookKey &key) {
  validate_key(key);
  std::unique_lock lock(storage_->mutex);
  if (!storage_->hooks.contains(key)) {
    return false;
  }
  invalidator_.invalidate_instruction(key.address);
  storage_->hooks.erase(key);
  return true;
}

std::optional<NativeHook> NativeHookRegistry::find(const HookKey &key) const {
  validate_key(key);
  std::shared_lock lock(storage_->mutex);
  const auto found = storage_->hooks.find(key);
  if (found == storage_->hooks.end()) {
    return std::nullopt;
  }
  return found->second;
}

std::size_t NativeHookRegistry::size() const {
  std::shared_lock lock(storage_->mutex);
  return storage_->hooks.size();
}

} // namespace gcnport
