#ifndef REGIONAL_SHARD_MANAGER_H
#define REGIONAL_SHARD_MANAGER_H

#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include "../common/TriBFTDefs.h"
#include "../consensus/VRFSelector.h"

namespace tribft {

/**
 * @brief Regional Shard Manager (Real Implementation)
 * 
 * Responsibilities (Single Responsibility Principle):
 * - Manage regional shard formation based on geographic location
 * - Handle node join/leave operations
 * - Maintain shard membership and leader information
 * - Perform dynamic shard rebalancing
 * 
 * Design Principles:
 * - SOLID: Single responsibility, open for extension
 * - KISS: Simple geographic clustering algorithm
 * - YAGNI: Only implement what's needed for regional sharding
 */
class RegionalShardManager {
public:
    RegionalShardManager();
    ~RegionalShardManager() = default;
    
    // 🔧 Get global shared instance (all nodes in simulation use this)
    static RegionalShardManager* getGlobalInstance();
    
    // ========================================================================
    // PUBLIC INTERFACE (Interface Segregation Principle)
    // ========================================================================
    
    /**
     * @brief Initialize the manager with configuration
     */
    void initialize(double shardRadius, int minShardSize, int maxShardSize);
    
    /**
     * @brief Add a node to appropriate shard based on location
     * @return Assigned shard ID
     */
    ShardID addNode(const NodeID& nodeID, const GeoCoord& location, ReputationScore reputation);
    
    /**
     * @brief Remove a node from its shard
     */
    void removeNode(const NodeID& nodeID);
    
    /**
     * @brief Update node's location (for mobile nodes)
     * @return New shard ID if changed, otherwise current shard ID
     */
    ShardID updateNodeLocation(const NodeID& nodeID, const GeoCoord& newLocation);
    
    /**
     * @brief Get shard ID for a given location
     */
    ShardID getShardForLocation(const GeoCoord& location) const;
    
    /**
     * @brief Get shard information
     */
    const ShardInfo* getShardInfo(ShardID shardID) const;
    
    /**
     * @brief Get node's current shard
     */
    ShardID getNodeShard(const NodeID& nodeID) const;
    
    /**
     * @brief Get all shards
     */
    std::vector<ShardInfo> getAllShards() const;
    
    /**
     * @brief Get shard leader
     */
    NodeID getShardLeader(ShardID shardID) const;
    
    /**
     * @brief Check if node is shard leader
     */
    bool isShardLeader(const NodeID& nodeID, ShardID shardID) const;
    
    /**
     * @brief Trigger leader election for a shard
     */
    void electLeader(ShardID shardID);
    
    /**
     * @brief Get node's current location
     * @return GeoCoord or default GeoCoord{0,0} if node not found
     */
    GeoCoord getNodeLocation(const NodeID& nodeID) const;
    
    // VRF Election and Consensus Group Management
    /**
     * @brief Elect consensus group for shard (using VRF)
     * @param shardID Shard ID
     * @param epoch Election epoch (blockHeight / epochBlocks)
     * @return Elected consensus group
     */
    ConsensusGroup electConsensusGroup(ShardID shardID, int epoch);
    
    /**
     * @brief Get current consensus group for shard
     */
    ConsensusGroup getCurrentConsensusGroup(ShardID shardID) const;
    
    /**
     * @brief Check if node is in consensus group
     */
    bool isInConsensusGroup(const NodeID& nodeID, ShardID shardID) const;
    
    /**
     * @brief Get node role
     */
    NodeRole getNodeRole(const NodeID& nodeID, ShardID shardID) const;
    
    /**
     * @brief Rebalance shards (merge small, split large)
     */
    void rebalanceShards();
    
    /**
     * @brief Get statistics
     */
    int getShardCount() const { return shards_.size(); }
    int getTotalNodes() const { return nodeShardMap_.size(); }
    
private:
    // ========================================================================
    // PRIVATE METHODS (Dependency Inversion Principle - internal logic)
    // ========================================================================
    
    /**
     * @brief Find nearest shard to a location
     */
    ShardID findNearestShard(const GeoCoord& location) const;
    
    /**
     * @brief Create a new shard at given location
     */
    ShardID createShard(const GeoCoord& centerPoint);
    
    /**
     * @brief Check if shard can accept more members
     */
    bool canAcceptMember(ShardID shardID) const;
    
    /**
     * @brief Elect leader based on reputation
     */
    NodeID electLeaderByReputation(ShardID shardID);
    
    /**
     * @brief Check if shard should be split
     */
    bool shouldSplitShard(ShardID shardID) const;
    
    /**
     * @brief Check if shard should be merged
     */
    bool shouldMergeShard(ShardID shardID) const;
    
    /**
     * @brief Split a large shard into two
     */
    void splitShard(ShardID shardID);
    
    /**
     * @brief Merge small shard with nearest neighbor
     */
    void mergeShard(ShardID shardID);
    
    /**
     * @brief Calculate optimal split point for a shard
     */
    GeoCoord calculateSplitPoint(const ShardInfo& shard) const;
    
    // ========================================================================
    // PRIVATE DATA MEMBERS (Open-Closed Principle - protected data)
    // ========================================================================
    
    std::map<ShardID, ShardInfo> shards_;                    // All shards
    std::map<NodeID, ShardID> nodeShardMap_;                 // Node to shard mapping
    std::map<NodeID, GeoCoord> nodeLocationMap_;             // Node locations
    std::map<NodeID, ReputationScore> nodeReputationMap_;    // Node reputations
    
    ShardID nextShardID_;                                    // Next available shard ID
    double shardRadius_;                                     // Shard coverage radius
    int minShardSize_;                                       // Minimum nodes per shard
    int maxShardSize_;                                       // Maximum nodes per shard
    
    // VRF selectors (one per shard)
    std::map<ShardID, VRFSelector*> vrfSelectors_;
    std::map<ShardID, ConsensusGroup> consensusGroups_;
    
    // Statistics
    int totalJoins_;
    int totalLeaves_;
    int totalSplits_;
    int totalMerges_;
};

} // namespace tribft

#endif // REGIONAL_SHARD_MANAGER_H



