#include "VRMManager.h"
#include <algorithm>

namespace tribft {

VRMManager::VRMManager() {
}

void VRMManager::initialize() {
    records_.clear();
    log("VRM Manager initialized");
}

void VRMManager::setLogCallback(LogCallback callback) {
    logCallback_ = callback;
}

// ============================================================================
// NODE MANAGEMENT
// ============================================================================

void VRMManager::registerNode(const NodeID& nodeID, ReputationScore initialScore) {
    if (records_.find(nodeID) != records_.end()) {
        log("Node " + nodeID + " already registered");
        return;
    }
    
    ReputationRecord record(nodeID);
    record.score = clampReputation(initialScore);
    record.lastUpdate = simTime();
    
    records_[nodeID] = record;
    log("Registered node " + nodeID + " with initial reputation " + std::to_string(initialScore));
}

void VRMManager::unregisterNode(const NodeID& nodeID) {
    auto it = records_.find(nodeID);
    if (it != records_.end()) {
        records_.erase(it);
        log("Unregistered node " + nodeID);
    }
}

bool VRMManager::isRegistered(const NodeID& nodeID) const {
    return records_.find(nodeID) != records_.end();
}

// ============================================================================
// REPUTATION QUERIES
// ============================================================================

ReputationScore VRMManager::getReputation(const NodeID& nodeID) const {
    auto it = records_.find(nodeID);
    if (it != records_.end()) {
        return it->second.score;
    }
    return Constants::INITIAL_REPUTATION;
}

const ReputationRecord* VRMManager::getRecord(const NodeID& nodeID) const {
    auto it = records_.find(nodeID);
    if (it != records_.end()) {
        return &(it->second);
    }
    return nullptr;
}

bool VRMManager::isReliable(const NodeID& nodeID) const {
    auto it = records_.find(nodeID);
    if (it != records_.end()) {
        return it->second.isReliable();
    }
    return false;
}

std::vector<NodeID> VRMManager::getTopNodes(int count) const {
    // Create sorted list of (nodeID, reputation) pairs
    std::vector<std::pair<NodeID, ReputationScore>> nodes;
    for (const auto& pair : records_) {
        nodes.push_back({pair.first, pair.second.score});
    }
    
    // Sort by reputation (descending)
    std::sort(nodes.begin(), nodes.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Extract top N node IDs
    std::vector<NodeID> result;
    int n = std::min(count, static_cast<int>(nodes.size()));
    for (int i = 0; i < n; i++) {
        result.push_back(nodes[i].first);
    }
    
    return result;
}

ReputationScore VRMManager::getAverageReputation() const {
    if (records_.empty()) {
        return Constants::INITIAL_REPUTATION;
    }
    
    double sum = 0.0;
    for (const auto& pair : records_) {
        sum += pair.second.score;
    }
    
    return sum / records_.size();
}

// ============================================================================
// REPUTATION UPDATES
// ============================================================================

void VRMManager::recordEvent(const NodeID& nodeID, ReputationEvent event) {
    auto it = records_.find(nodeID);
    if (it == records_.end()) {
        log("Cannot record event for unregistered node " + nodeID);
        return;
    }
    
    ReputationRecord& record = it->second;
    record.recentEvents.push_back(event);
    record.lastUpdate = simTime();
    
    // Apply marginal diminishing reward mechanism
    EventWeight weight = getEventWeight(event);
    double currentRep = record.getFinalReputation();
    double alpha = weight.getEffectiveWeight(currentRep);
    
    // Determine delta direction based on event type
    double delta = 0.0;
    switch (event) {
        // Positive events (apply marginal diminishing)
        case ReputationEvent::PROPOSE_VALID_BLOCK:
            delta = alpha;  // α = β/(1+R)
            record.validProposals++;
            record.totalProposals++;
            break;
            
        case ReputationEvent::VOTE_CORRECTLY:
            delta = alpha;
            record.correctVotes++;
            record.totalVotes++;
            break;
            
        case ReputationEvent::SUCCESSFUL_CONSENSUS:
            delta = alpha;
            break;
        
        case ReputationEvent::SUCCESSFUL_TX:
            delta = alpha;
            record.successfulTx++;
            break;
            
        case ReputationEvent::SUCCESSFUL_VOTE:
            delta = alpha;
            break;
            
        // Negative events (fixed penalty)
        case ReputationEvent::PROPOSE_INVALID_BLOCK:
            delta = -alpha;  // α = γ (fixed)
            record.totalProposals++;
            break;
            
        case ReputationEvent::VOTE_INCORRECTLY:
            delta = -alpha;
            record.totalVotes++;
            break;
            
        case ReputationEvent::TIMEOUT:
            delta = -alpha;
            break;
            
        case ReputationEvent::MALICIOUS_BEHAVIOR:
            delta = -alpha;
            break;
            
        case ReputationEvent::FAILED_CONSENSUS:
            delta = -alpha;
            break;
            
        case ReputationEvent::FAILED_TX:
            delta = -alpha;
            record.failedTx++;
            break;
            
        case ReputationEvent::FAILED_VOTE:
            delta = -alpha;
            break;
            
        default:
            delta = 0.0;
            break;
    }
    
    updateScore(nodeID, delta);
}

void VRMManager::updateForProposal(const NodeID& proposer, bool wasValid) {
    if (wasValid) {
        recordEvent(proposer, ReputationEvent::PROPOSE_VALID_BLOCK);
    } else {
        recordEvent(proposer, ReputationEvent::PROPOSE_INVALID_BLOCK);
    }
}

void VRMManager::updateForVote(const NodeID& voter, bool wasCorrect) {
    if (wasCorrect) {
        recordEvent(voter, ReputationEvent::VOTE_CORRECTLY);
    } else {
        recordEvent(voter, ReputationEvent::VOTE_INCORRECTLY);
    }
}

void VRMManager::updateForConsensusSuccess(const std::vector<NodeID>& participants) {
    for (const NodeID& nodeID : participants) {
        recordEvent(nodeID, ReputationEvent::SUCCESSFUL_CONSENSUS);
    }
}

void VRMManager::updateForConsensusFail(const std::vector<NodeID>& participants) {
    for (const NodeID& nodeID : participants) {
        recordEvent(nodeID, ReputationEvent::FAILED_CONSENSUS);
    }
}

void VRMManager::penalizeForTimeout(const NodeID& nodeID) {
    recordEvent(nodeID, ReputationEvent::TIMEOUT);
}

void VRMManager::penalizeForMalicious(const NodeID& nodeID) {
    recordEvent(nodeID, ReputationEvent::MALICIOUS_BEHAVIOR);
}

// ============================================================================
// PERIODIC MAINTENANCE
// ============================================================================

void VRMManager::applyDecay() {
    for (auto& pair : records_) {
        ReputationRecord& record = pair.second;
        
        // Apply decay: move reputation slightly towards initial value
        double target = Constants::INITIAL_REPUTATION;
        double decayRate = Constants::REPUTATION_DECAY_RATE;
        record.score = record.score * (1.0 - decayRate) + target * decayRate;
        
        record.score = clampReputation(record.score);
        record.lastUpdate = simTime();
    }
    
    log("Applied reputation decay to " + std::to_string(records_.size()) + " nodes");
}

void VRMManager::cleanupHistory(int maxEventsPerNode) {
    for (auto& pair : records_) {
        ReputationRecord& record = pair.second;
        
        if (record.recentEvents.size() > static_cast<size_t>(maxEventsPerNode)) {
            // Keep only recent events
            record.recentEvents.erase(
                record.recentEvents.begin(),
                record.recentEvents.begin() + (record.recentEvents.size() - maxEventsPerNode)
            );
        }
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

int VRMManager::getReliableNodeCount() const {
    int count = 0;
    for (const auto& pair : records_) {
        if (pair.second.isReliable()) {
            count++;
        }
    }
    return count;
}

VRMManager::Statistics VRMManager::getStatistics() const {
    Statistics stats;
    stats.totalNodes = records_.size();
    
    if (records_.empty()) {
        return stats;
    }
    
    double sum = 0.0;
    stats.maxScore = Constants::MIN_REPUTATION;
    stats.minScore = Constants::MAX_REPUTATION;
    
    for (const auto& pair : records_) {
        ReputationScore score = pair.second.score;
        sum += score;
        
        if (score > stats.maxScore) stats.maxScore = score;
        if (score < stats.minScore) stats.minScore = score;
        
        if (pair.second.isReliable()) {
            stats.reliableNodes++;
        }
    }
    
    stats.averageScore = sum / records_.size();
    
    return stats;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

EventWeight VRMManager::getEventWeight(ReputationEvent event) const {
    // TODO: Core implementation hidden - will be released after project completion
    return EventWeight(0.0, false);
}

void VRMManager::updateScore(const NodeID& nodeID, double delta) {
    // TODO: Core implementation hidden - will be released after project completion
}

ReputationScore VRMManager::clampReputation(ReputationScore score) const {
    if (score < Constants::MIN_REPUTATION) {
        return Constants::MIN_REPUTATION;
    }
    if (score > Constants::MAX_REPUTATION) {
        return Constants::MAX_REPUTATION;
    }
    return score;
}

void VRMManager::log(const std::string& message) {
    if (logCallback_) {
        logCallback_(message);
    }
}

} // namespace tribft



