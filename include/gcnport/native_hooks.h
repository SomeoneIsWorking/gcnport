// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

#include "gcnport/execution_types.h"
#include "gcnport/guest_context.h"

namespace gcnport {

struct ImageIdentity {
  std::array<std::uint8_t, 32> sha256{};

  [[nodiscard]] bool is_authenticated() const noexcept;
  auto operator<=>(const ImageIdentity &) const = default;
};

struct ExecutionIdentity {
  ImageIdentity image;
  std::uint64_t module_generation = 0;

  auto operator<=>(const ExecutionIdentity &) const = default;
};

struct HookKey {
  ExecutionIdentity identity;
  GuestAddress address = 0;

  [[nodiscard]] bool is_valid() const noexcept;
  auto operator<=>(const HookKey &) const = default;
};

enum class HookAction {
  ReturnToCaller,
  ContinueAtAddress,
  CallOriginalOnce,
};

struct HookResult {
  HookAction action = HookAction::ReturnToCaller;
  GuestAddress continuation = 0;

  [[nodiscard]] static HookResult return_to_caller() noexcept;
  [[nodiscard]] static HookResult continue_at(GuestAddress address) noexcept;
  [[nodiscard]] static HookResult call_original_once() noexcept;
};

using NativeHook = std::function<HookResult(GuestContext &)>;

class CodeInvalidator {
public:
  virtual ~CodeInvalidator() = default;
  virtual void invalidate_instruction(GuestAddress address) = 0;
};

class NativeHookRegistry {
public:
  explicit NativeHookRegistry(CodeInvalidator &invalidator);
  ~NativeHookRegistry();
  NativeHookRegistry(const NativeHookRegistry &) = delete;
  NativeHookRegistry(NativeHookRegistry &&) = delete;
  NativeHookRegistry &operator=(const NativeHookRegistry &) = delete;
  NativeHookRegistry &operator=(NativeHookRegistry &&) = delete;

  // Mutations happen at the backend's CPU safe point. The invalidator must not call back into this
  // registry while the mutation is in progress.
  void install(HookKey key, NativeHook hook);
  [[nodiscard]] bool remove(const HookKey &key);
  [[nodiscard]] std::optional<NativeHook> find(const HookKey &key) const;
  [[nodiscard]] std::size_t size() const;

private:
  class Storage;

  CodeInvalidator &invalidator_;
  std::unique_ptr<Storage> storage_;
};

} // namespace gcnport
