#include "VRFSelector.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <iostream>

namespace tribft {

VRFSelector::VRFSelector()
    : shardID_(-1)
    , lastEpoch_(-1)
{
}

void VRFSelector::initialize(ShardID shardID) {
    shardID_ = shardID;
    lastEpoch_ = -1;
    currentGroup_ = ConsensusGroup();
    nodeRoles_.clear();
    
    log("VRFSelector initialized for shard " + std::to_string(shardID));
}

void VRFSelector::setLogCallback(LogCallback callback) {
    logCallback_ = callback;
}

// ============================================================================
// Election Interface
// ============================================================================

ConsensusGroup VRFSelector::electConsensusGroup(
    const std::vector<NodeID>& candidates,
    const std::vector<NodeID>& rsuNodes,
    int groupSize,
    int redundantCount,
    uint64_t seed)
{
    // TODO: Core implementation hidden - will be released after project completion
    return ConsensusGroup();
}

bool VRFSelector::isInConsensusGroup(const NodeID& nodeID) const {
    for (const auto& node : currentGroup_.primaryNodes) {
        if (node == nodeID) return true;
    }
    return false;
}

bool VRFSelector::isRedundantNode(const NodeID& nodeID) const {
    for (const auto& node : currentGroup_.redundantNodes) {
        if (node == nodeID) return true;
    }
    return false;
}

NodeRole VRFSelector::getNodeRole(const NodeID& nodeID) const {
    auto it = nodeRoles_.find(nodeID);
    if (it != nodeRoles_.end()) {
        return it->second;
    }
    return NodeRole::ORDINARY;
}

// ============================================================================
// Rotation Management
// ============================================================================

bool VRFSelector::needsReelection(int currentEpoch) const {
    return currentEpoch > lastEpoch_;
}

void VRFSelector::updateEpoch(int epoch) {
    lastEpoch_ = epoch;
}

void VRFSelector::setCurrentGroup(const ConsensusGroup& group) {
    currentGroup_ = group;
    
    // Synchronously update nodeRoles_ mapping
    nodeRoles_.clear();
    for (const NodeID& nodeID : group.primaryNodes) {
        nodeRoles_[nodeID] = NodeRole::CONSENSUS_PRIMARY;
    }
    for (const NodeID& nodeID : group.redundantNodes) {
        nodeRoles_[nodeID] = NodeRole::CONSENSUS_REDUNDANT;
    }
}

// ============================================================================
// Internal Methods
// ============================================================================

double VRFSelector::calculateVRF(const NodeID& nodeID, uint64_t seed) const {
    // TODO: Core implementation hidden - will be released after project completion
    return 0.0;
}

std::vector<NodeID> VRFSelector::selectTopN(
    const std::vector<NodeID>& candidates,
    int count,
    uint64_t seed) const
{
    if (candidates.empty()) {
        return {};
    }
    
    // Calculate VRF value for each candidate
    std::vector<std::pair<double, NodeID>> vrfValues;
    for (const auto& nodeID : candidates) {
        double vrf = calculateVRF(nodeID, seed);
        vrfValues.push_back({vrf, nodeID});
    }
    
    // Sort by VRF value (descending)
    std::sort(vrfValues.begin(), vrfValues.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });
    
    // Select top N
    std::vector<NodeID> selected;
    int selectCount = std::min(count, static_cast<int>(vrfValues.size()));
    for (int i = 0; i < selectCount; ++i) {
        selected.push_back(vrfValues[i].second);
    }
    
    return selected;
}

void VRFSelector::log(const std::string& message) const {
    if (logCallback_) {
        logCallback_("[VRF-Shard" + std::to_string(shardID_) + "] " + message);
    }
}

} // namespace tribft
