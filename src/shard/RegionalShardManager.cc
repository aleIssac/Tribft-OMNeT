#include "RegionalShardManager.h"
#include <limits>
#include <cmath>

namespace tribft {

// 🔧 Global shared instance using anonymous namespace (safe within single process)
namespace {
    RegionalShardManager* globalShardManager = nullptr;
}

// Public accessor
RegionalShardManager* RegionalShardManager::getGlobalInstance() {
    if (!globalShardManager) {
        globalShardManager = new RegionalShardManager();
    }
    return globalShardManager;
}

RegionalShardManager::RegionalShardManager()
    : nextShardID_(0)
    , shardRadius_(Constants::REGIONAL_SHARD_RADIUS)
    , minShardSize_(Constants::MIN_SHARD_SIZE)
    , maxShardSize_(Constants::MAX_SHARD_SIZE)
    , totalJoins_(0)
    , totalLeaves_(0)
    , totalSplits_(0)
    , totalMerges_(0)
{
}

void RegionalShardManager::initialize(double shardRadius, int minShardSize, int maxShardSize) {
    shardRadius_ = shardRadius;
    minShardSize_ = minShardSize;
    maxShardSize_ = maxShardSize;
}

ShardID RegionalShardManager::addNode(const NodeID& nodeID, const GeoCoord& location, ReputationScore reputation) {
    // Check if node already exists
    if (nodeShardMap_.find(nodeID) != nodeShardMap_.end()) {
        return nodeShardMap_[nodeID];
    }
    
    // Store node information
    nodeLocationMap_[nodeID] = location;
    nodeReputationMap_[nodeID] = reputation;
    
    // Find appropriate shard
    ShardID shardID = getShardForLocation(location);
    
    if (shardID == -1) {
        // No suitable shard found, create new one
        shardID = createShard(location);
    }
    
    // Add node to shard
    ShardInfo& shard = shards_[shardID];
    shard.members.insert(nodeID);
    shard.lastUpdate = simTime();
    nodeShardMap_[nodeID] = shardID;
    
    // Elect leader if needed
    if (shard.leader.empty()) {
        electLeader(shardID);
    }
    
    // Check if shard needs to be split
    if (shouldSplitShard(shardID)) {
        splitShard(shardID);
    }
    
    totalJoins_++;
    return shardID;
}

void RegionalShardManager::removeNode(const NodeID& nodeID) {
    auto it = nodeShardMap_.find(nodeID);
    if (it == nodeShardMap_.end()) {
        return; // Node not found
    }
    
    ShardID shardID = it->second;
    ShardInfo& shard = shards_[shardID];
    
    // Remove from shard
    shard.members.erase(nodeID);
    shard.lastUpdate = simTime();
    
    // If removed node was leader, elect new leader
    if (shard.leader == nodeID) {
        shard.leader.clear();
        if (!shard.members.empty()) {
            electLeader(shardID);
        }
    }
    
    // Cleanup maps
    nodeShardMap_.erase(nodeID);
    nodeLocationMap_.erase(nodeID);
    nodeReputationMap_.erase(nodeID);
    
    // Check if shard should be merged or removed
    if (shard.members.empty()) {
        shards_.erase(shardID);
    } else if (shouldMergeShard(shardID)) {
        mergeShard(shardID);
    }
    
    totalLeaves_++;
}

ShardID RegionalShardManager::updateNodeLocation(const NodeID& nodeID, const GeoCoord& newLocation) {
    auto it = nodeShardMap_.find(nodeID);
    if (it == nodeShardMap_.end()) {
        return -1; // Node not found
    }
    
    ShardID currentShardID = it->second;
    nodeLocationMap_[nodeID] = newLocation;
    
    // Check if node is still within current shard
    const ShardInfo& currentShard = shards_[currentShardID];
    if (currentShard.contains(newLocation)) {
        return currentShardID; // No change needed
    }
    
    // Node moved out of shard, reassign
    removeNode(nodeID);
    ReputationScore reputation = nodeReputationMap_[nodeID];
    return addNode(nodeID, newLocation, reputation);
}

ShardID RegionalShardManager::getShardForLocation(const GeoCoord& location) const {
    ShardID bestShard = -1;
    double minDistance = std::numeric_limits<double>::max();
    
    for (const auto& pair : shards_) {
        ShardID shardID = pair.first;
        const ShardInfo& shard = pair.second;
        
        // Check if location is within shard radius
        if (shard.contains(location)) {
            // Check if shard can accept more members
            if (canAcceptMember(shardID)) {
                double distance = shard.centerPoint.distanceTo(location);
                if (distance < minDistance) {
                    minDistance = distance;
                    bestShard = shardID;
                }
            }
        }
    }
    
    return bestShard;
}

const ShardInfo* RegionalShardManager::getShardInfo(ShardID shardID) const {
    auto it = shards_.find(shardID);
    if (it != shards_.end()) {
        return &(it->second);
    }
    return nullptr;
}

ShardID RegionalShardManager::getNodeShard(const NodeID& nodeID) const {
    auto it = nodeShardMap_.find(nodeID);
    if (it != nodeShardMap_.end()) {
        return it->second;
    }
    return -1;
}

std::vector<ShardInfo> RegionalShardManager::getAllShards() const {
    std::vector<ShardInfo> result;
    result.reserve(shards_.size());
    for (const auto& pair : shards_) {
        result.push_back(pair.second);
    }
    return result;
}

NodeID RegionalShardManager::getShardLeader(ShardID shardID) const {
    auto it = shards_.find(shardID);
    if (it != shards_.end()) {
        return it->second.leader;
    }
    return "";
}

bool RegionalShardManager::isShardLeader(const NodeID& nodeID, ShardID shardID) const {
    auto it = shards_.find(shardID);
    if (it != shards_.end()) {
        return it->second.isLeader(nodeID);
    }
    return false;
}

void RegionalShardManager::electLeader(ShardID shardID) {
    auto it = shards_.find(shardID);
    if (it == shards_.end()) {
        return;
    }
    
    ShardInfo& shard = it->second;
    shard.leader = electLeaderByReputation(shardID);
    shard.lastUpdate = simTime();
}

GeoCoord RegionalShardManager::getNodeLocation(const NodeID& nodeID) const {
    auto it = nodeLocationMap_.find(nodeID);
    if (it != nodeLocationMap_.end()) {
        return it->second;
    }
    // Return default location if not found
    return GeoCoord{0.0, 0.0};
}

void RegionalShardManager::rebalanceShards() {
    // Check for shards that need splitting
    std::vector<ShardID> toSplit;
    for (const auto& pair : shards_) {
        if (shouldSplitShard(pair.first)) {
            toSplit.push_back(pair.first);
        }
    }
    
    for (ShardID shardID : toSplit) {
        splitShard(shardID);
    }
    
    // Check for shards that need merging
    std::vector<ShardID> toMerge;
    for (const auto& pair : shards_) {
        if (shouldMergeShard(pair.first)) {
            toMerge.push_back(pair.first);
        }
    }
    
    for (ShardID shardID : toMerge) {
        if (shards_.find(shardID) != shards_.end()) { // Check if still exists
            mergeShard(shardID);
        }
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

ShardID RegionalShardManager::findNearestShard(const GeoCoord& location) const {
    ShardID nearestShard = -1;
    double minDistance = std::numeric_limits<double>::max();
    
    for (const auto& pair : shards_) {
        double distance = pair.second.centerPoint.distanceTo(location);
        if (distance < minDistance) {
            minDistance = distance;
            nearestShard = pair.first;
        }
    }
    
    return nearestShard;
}

ShardID RegionalShardManager::createShard(const GeoCoord& centerPoint) {
    ShardID shardID = nextShardID_++;
    
    ShardInfo shard;
    shard.shardID = shardID;
    shard.level = ShardLevel::REGIONAL;
    shard.centerPoint = centerPoint;
    shard.radius = shardRadius_;
    shard.lastUpdate = simTime();
    
    shards_[shardID] = shard;
    return shardID;
}

bool RegionalShardManager::canAcceptMember(ShardID shardID) const {
    auto it = shards_.find(shardID);
    if (it != shards_.end()) {
        return it->second.getMemberCount() < maxShardSize_;
    }
    return false;
}

NodeID RegionalShardManager::electLeaderByReputation(ShardID shardID) {
    // TODO: Core implementation hidden - will be released after project completion
    return "";
}

bool RegionalShardManager::shouldSplitShard(ShardID shardID) const {
    auto it = shards_.find(shardID);
    if (it != shards_.end()) {
        return it->second.getMemberCount() > maxShardSize_;
    }
    return false;
}

bool RegionalShardManager::shouldMergeShard(ShardID shardID) const {
    auto it = shards_.find(shardID);
    if (it != shards_.end()) {
        return it->second.getMemberCount() < minShardSize_;
    }
    return false;
}

void RegionalShardManager::splitShard(ShardID shardID) {
    auto it = shards_.find(shardID);
    if (it == shards_.end()) {
        return;
    }
    
    ShardInfo& originalShard = it->second;
    if (originalShard.members.size() <= minShardSize_) {
        return; // Too small to split
    }
    
    // Calculate split point
    GeoCoord splitPoint = calculateSplitPoint(originalShard);
    
    // Create new shard
    ShardID newShardID = createShard(splitPoint);
    
    // Redistribute members based on proximity
    std::vector<NodeID> membersToMove;
    for (const NodeID& nodeID : originalShard.members) {
        auto locIt = nodeLocationMap_.find(nodeID);
        if (locIt != nodeLocationMap_.end()) {
            double distToOriginal = originalShard.centerPoint.distanceTo(locIt->second);
            double distToNew = splitPoint.distanceTo(locIt->second);
            
            if (distToNew < distToOriginal) {
                membersToMove.push_back(nodeID);
            }
        }
    }
    
    // Move members to new shard
    ShardInfo& newShard = shards_[newShardID];
    for (const NodeID& nodeID : membersToMove) {
        originalShard.members.erase(nodeID);
        newShard.members.insert(nodeID);
        nodeShardMap_[nodeID] = newShardID;
    }
    
    // Elect leaders for both shards
    electLeader(shardID);
    electLeader(newShardID);
    
    totalSplits_++;
}

void RegionalShardManager::mergeShard(ShardID shardID) {
    auto it = shards_.find(shardID);
    if (it == shards_.end()) {
        return;
    }
    
    // Find nearest shard to merge with
    ShardID nearestShard = findNearestShard(it->second.centerPoint);
    if (nearestShard == -1 || nearestShard == shardID) {
        return;
    }
    
    // Move all members to nearest shard
    ShardInfo& targetShard = shards_[nearestShard];
    for (const NodeID& nodeID : it->second.members) {
        targetShard.members.insert(nodeID);
        nodeShardMap_[nodeID] = nearestShard;
    }
    
    // Remove original shard
    shards_.erase(shardID);
    
    // Re-elect leader
    electLeader(nearestShard);
    
    totalMerges_++;
}

GeoCoord RegionalShardManager::calculateSplitPoint(const ShardInfo& shard) const {
    if (shard.members.empty()) {
        return shard.centerPoint;
    }
    
    // Calculate centroid of member locations
    double avgLat = 0.0;
    double avgLon = 0.0;
    int count = 0;
    
    for (const NodeID& nodeID : shard.members) {
        auto it = nodeLocationMap_.find(nodeID);
        if (it != nodeLocationMap_.end()) {
            avgLat += it->second.latitude;
            avgLon += it->second.longitude;
            count++;
        }
    }
    
    if (count > 0) {
        return GeoCoord(avgLat / count, avgLon / count);
    }
    
    return shard.centerPoint;
}

// ============================================================================
// VRF Election and Consensus Group Management
// ============================================================================

ConsensusGroup RegionalShardManager::electConsensusGroup(ShardID shardID, int epoch) {
    // TODO: Core implementation hidden - will be released after project completion
    return ConsensusGroup();
}

ConsensusGroup RegionalShardManager::getCurrentConsensusGroup(ShardID shardID) const {
    auto it = consensusGroups_.find(shardID);
    if (it != consensusGroups_.end()) {
        return it->second;
    }
    return ConsensusGroup();
}

bool RegionalShardManager::isInConsensusGroup(const NodeID& nodeID, ShardID shardID) const {
    auto selectorIt = vrfSelectors_.find(shardID);
    if (selectorIt != vrfSelectors_.end()) {
        return selectorIt->second->isInConsensusGroup(nodeID);
    }
    return false;
}

NodeRole RegionalShardManager::getNodeRole(const NodeID& nodeID, ShardID shardID) const {
    auto selectorIt = vrfSelectors_.find(shardID);
    if (selectorIt != vrfSelectors_.end()) {
        return selectorIt->second->getNodeRole(nodeID);
    }
    return NodeRole::ORDINARY;
}

} // namespace tribft
