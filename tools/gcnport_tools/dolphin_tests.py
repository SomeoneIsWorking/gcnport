"""Required native Dolphin regression inventory and fail-closed GoogleTest evidence."""

from __future__ import annotations

import re
from collections import Counter

from .host import SUPPORTED_TARGETS, HostTarget

SHIPPING_JIT_TEST = "GcnPortRuntime.ShippingJitCacheHookOriginalAndInvalidation"
COMMON_TESTS = (
    SHIPPING_JIT_TEST,
    "GcnPortRuntime.BootAuthenticatedImageAppliesGameCubeOsInitRegisters",
    "GcnPortRuntime.BootAuthenticatedImageDefaultsToNoGameCubeOsInit",
    "GcnPortRuntime.ExecuteJitBlockAdvancesCoreTimingAndRunsExactlyOneBlock",
    "GcnPortRuntime.BootOwnsTheSubsystemsDolphinDereferencesUnchecked",
    "GcnPortRuntime.ExecuteJitBlocksChainsBlocksAndRestoresTheOneBlockCap",
    "GcnPortRuntime.DiscImageRefusalsNeverBootSilentlyWithoutTheDisc",
    "GcnPortRuntime.DiscRegionAndShippedSettingsConfigureTheConsole",
    "GcnPortRuntime.MediaInitOwnsHeadlessVideoAndDspOrRefuses",
    "GcnPortRuntime.PublicAdapterBootExecuteOriginalAndTypedFallback",
    "GcnPortRuntime.HookCallsOriginalSynchronouslyThenResumesNativeWork",
    "GcnPortRuntime.ClassifyFallbackReasonMatchesStaticOpcodeTables",
    "GcnPortRuntime.FallbackAccountingNamesItsSitesAndReportsItsOwnTruncation",
    "GcnPortRuntime.BootAuthenticatedImageAppliesGameCubeHardwareInitMmio",
    "GcnPortRuntime.BootAuthenticatedImageWithoutHardwareInitFaultsOnMmioAccess",
    "MsgHandlerTest.AssertForwardsZeroOneAndTwoFormatArguments",
    "MsgHandlerTest.SuccessfulConditionDoesNotEvaluateArgumentsOrReachSink",
    "MsgHandlerTest.HandlerRegistrationReturnsAndRestoresPreviousOwner",
    "MemberOffset.AddressesLiveNonStandardSubobjects",
    "MemberOffsetDeathTest.RejectsForeignSubobject",
    "SocketHandle.NativeDescriptorIdentity",
    "AMMediaboardSocket.NativeIdentitySurvivesAcquisitionAndMapping",
    "AMMediaboardSocket.FailedNativeAcquisitionNeverPublishesOrConfigures",
    "AMMediaboardSocket.UnavailableGuestSlotNeverAcquiresNativeResource",
    "SSLTransportTest.BinaryPayloadRetryAndEndOfStream",
    "SSLTransportTest.InvalidContextAndWriteBackpressure",
    "SSLTransportTest.ClosedSocketFailsAndResetIsDistinct",
)
POSIX_TESTS = ("SSLTransportTest.MatchesMbedTLSReference",)
X64_TESTS = (
    "DSPJitState.RegisterOperandsAddressTheLiveState",
    "DSPJitState.ClearProductWritesOnlyTheProductRegister",
    "Jit64.StateOffsetsAddressTheLiveState",
)


def required_tests(host: HostTarget) -> tuple[str, ...]:
    if (host.operating_system, host.architecture) not in SUPPORTED_TARGETS:
        raise RuntimeError(f"no Dolphin regression inventory for {host}")
    return (
        COMMON_TESTS
        + (POSIX_TESTS if host.operating_system != "windows" else ())
        + (X64_TESTS if host.architecture == "x64" else ())
    )


def validate_listing(listing: str, expected: tuple[str, ...]) -> None:
    suite = ""
    found: Counter[str] = Counter()
    for line in listing.splitlines():
        if match := re.fullmatch(r"([\w/]+\.)\s*(?:#.*)?", line):
            suite = match[1]
        elif suite and (match := re.fullmatch(r"\s+([\w/]+)\s*(?:#.*)?", line)):
            found[suite + match[1]] += 1
    invalid = {name: found[name] for name in expected if found[name] != 1}
    if invalid:
        raise RuntimeError(
            f"Dolphin test discovery scanned {sum(found.values())} entries; "
            f"required tests must occur once, observed {invalid}"
        )


def validate_result(output: str, expected: tuple[str, ...]) -> None:
    if re.search(r"\[\s*(?:FAILED|SKIPPED)\s*\]", output):
        raise RuntimeError("required Dolphin regressions reported a failure or skip")
    for status in ("RUN", "OK"):
        names = re.findall(rf"^\[\s*{status}\s*\]\s+(\S+)(?:\s.*)?$", output, re.MULTILINE)
        if Counter(names) != Counter(expected):
            raise RuntimeError(
                f"Dolphin {status} inventory mismatch: expected {list(expected)}, observed {names}"
            )
    summaries = re.findall(r"^\[\s*PASSED\s*\]\s+(\d+) tests?\.$", output, re.MULTILINE)
    if summaries != [str(len(expected))]:
        raise RuntimeError(
            f"Dolphin passing-count mismatch: expected {len(expected)}, observed {summaries}"
        )
