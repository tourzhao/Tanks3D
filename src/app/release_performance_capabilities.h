#ifndef TANKS3D_APP_RELEASE_PERFORMANCE_CAPABILITIES_H
#define TANKS3D_APP_RELEASE_PERFORMANCE_CAPABILITIES_H

#include "release_performance_log.h"

#include <ostream>
#include <string>

namespace tanks3d::app
{
inline constexpr const char *kReleasePerformanceCapabilitiesArgument =
    "--self-test=release-performance-capabilities";
inline constexpr const char *kReleasePerformanceCapabilitiesSchema =
    "tanks3d-release-performance-capabilities-v1";
inline constexpr const char *kReleasePerformanceStartMarker =
    "TANKS3D_PERFORMANCE_START";
inline constexpr const char *kReleasePerformanceCompleteMarker =
    "TANKS3D_PERFORMANCE_COMPLETE";

struct ReleasePerformanceCapabilityCheck
{
    bool success = false;
    std::string error;
};

ReleasePerformanceCapabilityCheck checkReleasePerformanceCapabilities(
    const std::string &sourceCommit, const std::string &sourceTag);

inline void writeReleasePerformanceCapabilities(
    std::ostream &output, const std::string &sourceCommit,
    const std::string &sourceTag)
{
    output << "{\n"
           << "  \"schema\": \"" << kReleasePerformanceCapabilitiesSchema
           << "\",\n"
           << "  \"telemetry_schema\": \"" << kReleasePerformanceLogSchema
           << "\",\n"
           << "  \"producer\": \"" << kReleasePerformanceProducer
           << "\",\n"
           << "  \"quick_start_argument\": \""
           << kReleasePerformanceQuickStartArgument << "\",\n"
           << "  \"log_argument_prefix\": \""
           << kReleasePerformanceLogArgumentPrefix << "\",\n"
           << "  \"candidate_sha256_argument_prefix\": \""
           << kReleasePerformanceCandidateArgumentPrefix << "\",\n"
           << "  \"session_nonce_argument_prefix\": \""
           << kReleasePerformanceNonceArgumentPrefix << "\",\n"
           << "  \"duration_argument_prefix\": \""
           << kReleasePerformanceDurationArgumentPrefix << "\",\n"
           << "  \"start_marker\": \"" << kReleasePerformanceStartMarker
           << "\",\n"
           << "  \"complete_marker\": \""
           << kReleasePerformanceCompleteMarker << "\",\n"
           << "  \"default_duration_seconds\": "
           << kDefaultReleasePerformanceDurationSeconds << ",\n"
           << "  \"maximum_duration_seconds\": "
           << kMaximumReleasePerformanceDurationSeconds << ",\n"
           << "  \"sample_interval_microseconds\": "
           << kReleasePerformanceIntervalMicroseconds << ",\n"
           << "  \"clock\": \"" << kReleasePerformanceClock << "\",\n"
           << "  \"memory_metric\": \""
           << kReleasePerformanceMemoryMetric << "\",\n"
           << "  \"memory_unit\": \"" << kReleasePerformanceMemoryUnit
           << "\",\n"
           << "  \"app_states\": [\"gameplay\", \"settlement\", "
              "\"high_score\"],\n"
           << "  \"source_commit\": \"" << sourceCommit << "\",\n"
           << "  \"source_tag\": \"" << sourceTag << "\",\n"
           << "  \"self_check\": \"PASS\"\n"
           << "}\n";
}
} // namespace tanks3d::app

#endif
