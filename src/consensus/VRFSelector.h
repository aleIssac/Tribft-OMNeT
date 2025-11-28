#ifndef VRF_SELECTOR_H
#define VRF_SELECTOR_H

#include <vector>
#include <map>
#include <functional>
#include "../common/TriBFTDefs.h"

namespace tribft {

/**
 * @brief Node role
 */
enum class NodeRole {
    ORDINARY = 0,       // Ordinary node: data contribution
    CONSENSUS_PRIMARY,  // Consensus primary: voting
    CONSENSUS_REDUNDANT,// Redundant node: hot backup
    RSU_PERMANENT       // RSU: permanent consensus member
};

/**
 * @brief Consensus group structure
 */
struct ConsensusGroup {
    std::vector<NodeID> primaryNodes;    // Primary nodes (participate in voting)
    std::vector<NodeID> redundantNodes;  // Redundant nodes (sync only)
    int rsuCount;                        // Number of RSU nodes
    int vehicleCount;                    // Number of vehicle nodes
    int epoch;                           // Election epoch
    
    ConsensusGroup() : rsuCount(0), vehicleCount(0), epoch(0) {}
    
    /**
     * @brief Check RSU ratio constraint
     * Paper requirement: N_RSU >= N_total / 3
     */
    bool satisfiesRSUConstraint() const {
        int total = primaryNodes.size();
        return rsuCount >= (total / 3);
    }
    
    /**
     * @brief Get total consensus group size (primary + redundant)
     */
    int getTotalSize() const {
        return primaryNodes.size() + redundantNodes.size();
    }
};

/**
 * @brief VRF Selector (Verifiable Random Function)
 * 
 * Features:
 * - Elect consensus group from trusted nodes
 * - Ensure RSU ratio >= 33%
 * - Manage redundant nodes
 * - Periodic rotation (every N blocks)
 * 
 * Design Principles:
 * - KISS: Simplified VRF as hash-based pseudo-random election
 * - SOLID: Single responsibility for election logic
 */
class VRFSelector {
public:
    // Callback type
    using LogCallback = std::function<void(const std::string&)>;
    
    VRFSelector();
    ~VRFSelector() = default;
    
    // ========================================================================
    // Initialization
    // ========================================================================
    
    /**
     * @brief Initialize VRF selector
     */
    void initialize(ShardID shardID);
    
    /**
     * @brief Set logging callback
     */
    void setLogCallback(LogCallback callback);
    
    // ========================================================================
    // Election Interface
    // ========================================================================
    
    /**
     * @brief Elect consensus group
     * @param candidates Candidate node list (must be trusted level)
     * @param rsuNodes RSU node list
     * @param groupSize Consensus group size (default 15)
     * @param redundantCount Redundant node count (default 5)
     * @param seed Random seed (usually block hash + epoch)
     * @return Elected consensus group
     */
    ConsensusGroup electConsensusGroup(
        const std::vector<NodeID>& candidates,
        const std::vector<NodeID>& rsuNodes,
        int groupSize = 15,
        int redundantCount = 5,
        uint64_t seed = 0
    );
    
    /**
     * @brief Check if node is in consensus group
     */
    bool isInConsensusGroup(const NodeID& nodeID) const;
    
    /**
     * @brief Check if node is a redundant node
     */
    bool isRedundantNode(const NodeID& nodeID) const;
    
    /**
     * @brief Get current consensus group
     */
    const ConsensusGroup& getCurrentGroup() const { return currentGroup_; }
    
    /**
     * @brief Set current consensus group (for external location-based election)
     */
    void setCurrentGroup(const ConsensusGroup& group);
    
    /**
     * @brief Get node role
     */
    NodeRole getNodeRole(const NodeID& nodeID) const;
    
    // ========================================================================
    // Rotation Management
    // ========================================================================
    
    /**
     * @brief Check if re-election is needed
     * @param currentEpoch Current epoch (blockHeight / epochBlocks)
     * @return true if re-election needed
     */
    bool needsReelection(int currentEpoch) const;
    
    /**
     * @brief Update epoch
     */
    void updateEpoch(int epoch);
    
private:
    // ========================================================================
    // Internal Methods
    // ========================================================================
    
    /**
     * @brief Calculate VRF value (simplified: hash-based)
     */
    double calculateVRF(const NodeID& nodeID, uint64_t seed) const;
    
    /**
     * @brief Select N nodes from candidates
     */
    std::vector<NodeID> selectTopN(
        const std::vector<NodeID>& candidates,
        int count,
        uint64_t seed
    ) const;
    
    /**
     * @brief Log output
     */
    void log(const std::string& message) const;
    
    // ========================================================================
    // Data Members
    // ========================================================================
    
    ShardID shardID_;
    ConsensusGroup currentGroup_;
    std::map<NodeID, NodeRole> nodeRoles_;
    int lastEpoch_;
    
    LogCallback logCallback_;
};

} // namespace tribft

#endif // VRF_SELECTOR_H
