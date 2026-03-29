#ifndef ATTACKER_NODE_H
#define ATTACKER_NODE_H

#include <optional>
#include <random>

#include "../common/sim-types.h"

namespace lorasec {

class AttackerNode
{
  public:
    explicit AttackerNode(const AttackConfig& config, uint32_t seed);

    bool ShouldAttackAt(uint32_t currentSec) const;
    std::optional<HelloMessage> AttackA_MetricSpoofing(uint32_t currentSec, uint32_t fakeSeq, bool usePatch) const;
    std::optional<HelloMessage> AttackB_MetricReplay(uint32_t currentSec,
                                                     uint32_t lastSeenSeq,
                                                     uint32_t lastSeenMetricVersion,
                                                     bool usePatch) const;
    bool AttackC_SelectiveForwarding();

    const AttackConfig& GetConfig() const;

  private:
    AttackConfig m_config;
    mutable std::mt19937 m_rng;
    std::bernoulli_distribution m_dropDistribution;
};

}

#endif
