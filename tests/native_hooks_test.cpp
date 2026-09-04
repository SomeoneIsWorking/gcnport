// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "gcnport/native_hooks.h"
#include "gcnport/original_calls.h"
#include "test_support.h"

namespace {

using namespace gcnport;

ImageIdentity image(std::uint8_t marker) {
  ImageIdentity identity;
  identity.sha256.front() = marker;
  return identity;
}

HookKey key(std::uint8_t marker, std::uint64_t generation, GuestAddress address) {
  return HookKey{
      .identity = {.image = image(marker), .module_generation = generation},
      .address = address,
  };
}

class RecordingInvalidator final : public CodeInvalidator {
public:
  void invalidate_instruction(GuestAddress address) override { addresses.push_back(address); }
  std::vector<GuestAddress> addresses;
};

class MemorylessContext final : public GuestContext {
public:
  [[nodiscard]] GuestAddress program_counter() const noexcept override { return pc; }
  void set_program_counter(GuestAddress value) noexcept override { pc = value; }
  [[nodiscard]] GuestAddress link_register() const noexcept override { return lr; }
  void set_link_register(GuestAddress value) noexcept override { lr = value; }
  [[nodiscard]] std::uint32_t general_register(std::size_t index) const override {
    return gpr.at(index);
  }
  void set_general_register(std::size_t index, std::uint32_t value) override {
    gpr.at(index) = value;
  }
  bool read_memory(GuestAddress /*address*/, std::span<std::byte> /*destination*/) override {
    return false;
  }
  bool write_memory(GuestAddress /*address*/, std::span<const std::byte> /*source*/) override {
    return false;
  }

  GuestAddress pc = 0;
  GuestAddress lr = 0;
  std::array<std::uint32_t, 32> gpr{};
};

void hooks_are_scoped_by_image_generation_and_address() {
  RecordingInvalidator invalidator;
  NativeHookRegistry registry(invalidator);
  const HookKey first = key(1, 7, 0x802e'0390);
  const HookKey other_generation = key(1, 8, 0x802e'0390);
  const HookKey other_image = key(2, 7, 0x802e'0390);
  bool invoked = false;

  registry.install(first, [&invoked](GuestContext &) {
    invoked = true;
    return HookResult::call_original_once();
  });

  GCPORT_REQUIRE(registry.size() == 1);
  GCPORT_REQUIRE(invalidator.addresses == std::vector<GuestAddress>{0x802e'0390});
  GCPORT_REQUIRE(registry.find(other_generation) == std::nullopt);
  GCPORT_REQUIRE(registry.find(other_image) == std::nullopt);
  const auto hook = registry.find(first);
  GCPORT_REQUIRE(hook.has_value());
  if (!hook.has_value()) {
    return;
  }
  MemorylessContext context;
  GCPORT_REQUIRE((*hook)(context).action == HookAction::CallOriginalOnce);
  GCPORT_REQUIRE(invoked);

  registry.install(first, [](GuestContext &) { return HookResult::continue_at(0x8000'4000); });
  GCPORT_REQUIRE(registry.size() == 1);
  GCPORT_REQUIRE(registry.remove(first));
  GCPORT_REQUIRE(!registry.remove(first));
  GCPORT_REQUIRE(invalidator.addresses ==
                 std::vector<GuestAddress>({0x802e'0390, 0x802e'0390, 0x802e'0390}));
}

void original_call_ticket_suppresses_exactly_one_matching_entry() {
  RecordingInvalidator invalidator;
  OriginalCallCoordinator calls(invalidator);
  const HookKey target = key(3, 11, 0x8000'1000);
  const HookKey wrong_generation = key(3, 12, 0x8000'1000);

  const OriginalCallTicket ticket = calls.begin(target);
  GCPORT_REQUIRE(calls.pending_count() == 1);
  GCPORT_REQUIRE(!calls.claim_entry(ticket, wrong_generation));
  GCPORT_REQUIRE(calls.claim_entry(ticket, target));
  GCPORT_REQUIRE(!calls.claim_entry(ticket, target));
  calls.complete(ticket);

  GCPORT_REQUIRE(calls.pending_count() == 0);
  GCPORT_REQUIRE(invalidator.addresses == std::vector<GuestAddress>({0x8000'1000, 0x8000'1000}));
}

void original_call_must_be_claimed_before_completion() {
  RecordingInvalidator invalidator;
  OriginalCallCoordinator calls(invalidator);
  const OriginalCallTicket ticket = calls.begin(key(4, 1, 0x8000'2000));
  bool rejected = false;
  try {
    calls.complete(ticket);
  } catch (const std::logic_error &) {
    rejected = true;
  }
  GCPORT_REQUIRE(rejected);
  calls.cancel(ticket);
  GCPORT_REQUIRE(calls.pending_count() == 0);
}

void unauthenticated_identity_is_rejected() {
  RecordingInvalidator invalidator;
  NativeHookRegistry registry(invalidator);
  bool rejected = false;
  try {
    registry.install(HookKey{.identity = {}, .address = 0x8000'3000},
                     [](GuestContext &) { return HookResult::return_to_caller(); });
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  GCPORT_REQUIRE(rejected);
  GCPORT_REQUIRE(invalidator.addresses.empty());
}

} // namespace

int main() {
  return gcnport::test::run_test_main("native_hooks", [] {
    hooks_are_scoped_by_image_generation_and_address();
    original_call_ticket_suppresses_exactly_one_matching_entry();
    original_call_must_be_claimed_before_completion();
    unauthenticated_identity_is_rejected();
  });
}
