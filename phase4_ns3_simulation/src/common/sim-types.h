#ifndef LORASEC_SIM_TYPES_H
#define LORASEC_SIM_TYPES_H

#include <cstdint>
#include <string>
#include <vector>

namespace lorasec {

enum class TopologyType
{
    LINEAR,
    TREE,
    GRID
};

enum class AttackType
{
    NONE,
    SPOOFING,
    REPLAY,
    SELECTIVE
};

enum class SecurityMode
{
    BASELINE,
    PATCHED,
    METRICVERSION
};

struct HelloMessage
{
    uint32_t claimedSrc;
    uint32_t sender;
    uint32_t metric;
    uint32_t seq;
    bool authenticated;
    uint32_t metricVersion;
};

struct DataPacket
{
    uint32_t id;
    uint32_t src;
    uint32_t dst;
    uint32_t createdAtSec;
    uint32_t payloadBytes;
};

struct RouteEntry
{
    uint32_t destination;
    uint32_t metric;
    uint32_t nextHop;
    uint32_t seq;
    uint32_t metricVersion;
};

struct TopologySpec
{
    TopologyType type;
    uint32_t nodeCount;
    std::vector<std::vector<uint32_t>> adjacency;
};

struct AttackConfig
{
    AttackType type{AttackType::NONE};
    uint32_t attackerNode{0};
    uint32_t victimNode{0};
    uint32_t frequencySec{10};
    double selectiveDropRate{0.5};
};

}

#endif
