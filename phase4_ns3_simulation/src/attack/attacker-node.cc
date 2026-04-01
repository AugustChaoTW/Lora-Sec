#include "attacker-node.h"

namespace lorasec {

AttackerNode::AttackerNode(const AttackConfig& config, uint32_t seed)
    : m_config(config), m_rng(seed), m_dropDistribution(config.selectiveDropRate)
{
}

bool
AttackerNode::ShouldAttackAt(uint32_t currentSec) const
{
    if (m_config.type == AttackType::NONE)
    {
        return false;
    }
    if (m_config.frequencySec == 0)
    {
        return true;
    }
    return currentSec % m_config.frequencySec == 0;
}

std::optional<HelloMessage>
AttackerNode::AttackA_MetricSpoofing(uint32_t currentSec,
                                     uint32_t fakeSeq,
                                     SecurityMode securityMode) const
{
    (void) securityMode;

    if (m_config.type != AttackType::SPOOFING)
    {
        return std::nullopt;
    }

    uint32_t forgedVersion = currentSec / 20;
    return HelloMessage{m_config.victimNode,
                        m_config.attackerNode,
                        0,
                        fakeSeq,
                        false,
                        forgedVersion};
}

std::optional<HelloMessage>
AttackerNode::AttackB_MetricReplay(uint32_t currentSec,
                                   uint32_t lastSeenSeq,
                                   uint32_t lastSeenMetricVersion,
                                   SecurityMode securityMode) const
{
    if (m_config.type != AttackType::REPLAY)
    {
        return std::nullopt;
    }

    uint32_t replayedSeq = lastSeenSeq;
    uint32_t replayedMetricVersion = (lastSeenMetricVersion > 1) ? lastSeenMetricVersion - 2 : 0;

    return HelloMessage{m_config.victimNode,
                        m_config.attackerNode,
                        0,
                        replayedSeq,
                        securityMode == SecurityMode::PATCHED,
                        replayedMetricVersion};
}

bool
AttackerNode::AttackC_SelectiveForwarding()
{
    if (m_config.type != AttackType::SELECTIVE)
    {
        return false;
    }
    return m_dropDistribution(m_rng);
}

const AttackConfig&
AttackerNode::GetConfig() const
{
    return m_config;
}

}
