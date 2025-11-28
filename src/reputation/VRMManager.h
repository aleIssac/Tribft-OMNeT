#ifndef VRM_MANAGER_H
#define VRM_MANAGER_H

#include <map>
#include <functional>
#include "../common/TriBFTDefs.h"

namespace tribft {

/**
 * @brief Vehicle Reputation Management (VRM) System
 * 
 * Responsibilities:
 * - Track and update node reputation scores
 * - Evaluate node behavior (proposals, votes, participation)
 * - Provide reputation-based node selection
 * - Apply rewards and penalties based on actions
 * 
 * Design Principles:
 * - SOLID: Single responsibility for reputation management
 * - KISS: Simple reward/penalty system
 * - YAGNI: Essential reputation features only
 */
class VRMManager {
public:
    // Callback for logging
    using LogCallback = std::function<void(const std::string&)>;
    
    VRMManager();
    ~VRMManager() = default;
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    /**
     * @brief Initialize the VRM manager
     */
    void initialize();
    
    /**
     * @brief Set logging callback
     */
    void setLogCallback(LogCallback callback);
    
    // ========================================================================
    // NODE MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Register a new node
     */
    void registerNode(const NodeID& nodeID, ReputationScore initialScore = Constants::INITIAL_REPUTATION);
    
    /**
     * @brief Unregister a node
     */
    void unregisterNode(const NodeID& nodeID);
    
    /**
     * @brief Check if node is registered
     */
    bool isRegistered(const NodeID& nodeID) const;
    
    // ========================================================================
    // REPUTATION QUERIES
    // ========================================================================
    
    /**
     * @brief Get node's reputation score
     */
    ReputationScore getReputation(const NodeID& nodeID) const;
    
    /**
     * @brief Get node's reputation record
     */
    const ReputationRecord* getRecord(const NodeID& nodeID) const;
    
    /**
     * @brief Check if node is reliable (reputation >= threshold)
     */
    bool isReliable(const NodeID& nodeID) const;
    
    /**
     * @brief Get top N nodes by reputation
     */
    std::vector<NodeID> getTopNodes(int count) const;
    
    /**
     * @brief Get average reputation of all nodes
     */
    ReputationScore getAverageReputation() const;
    
    // ========================================================================
    // REPUTATION UPDATES (Event-based)
    // ========================================================================
    
    /**
     * @brief Record a reputation event
     */
    void recordEvent(const NodeID& nodeID, ReputationEvent event);
    
    /**
     * @brief Update reputation based on proposal outcome
     */
    void updateForProposal(const NodeID& proposer, bool wasValid);
    
    /**
     * @brief Update reputation based on vote correctness
     */
    void updateForVote(const NodeID& voter, bool wasCorrect);
    
    /**
     * @brief Update reputation for successful consensus participation
     */
    void updateForConsensusSuccess(const std::vector<NodeID>& participants);
    
    /**
     * @brief Update reputation for failed consensus
     */
    void updateForConsensusFail(const std::vector<NodeID>& participants);
    
    /**
     * @brief Penalize for timeout or no response
     */
    void penalizeForTimeout(const NodeID& nodeID);
    
    /**
     * @brief Penalize for malicious behavior
     */
    void penalizeForMalicious(const NodeID& nodeID);
    
    // ========================================================================
    // PERIODIC MAINTENANCE
    // ========================================================================
    
    /**
     * @brief Apply reputation decay (called periodically)
     */
    void applyDecay();
    
    /**
     * @brief Clean up old events from history
     */
    void cleanupHistory(int maxEventsPerNode = 100);
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    int getNodeCount() const { return records_.size(); }
    int getReliableNodeCount() const;
    
    struct Statistics {
        int totalNodes{0};
        int reliableNodes{0};
        ReputationScore averageScore{0.0};
        ReputationScore maxScore{0.0};
        ReputationScore minScore{1.0};
    };
    
    Statistics getStatistics() const;
    
private:
    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================
    
    /**
     * @brief Update reputation score
     */
    void updateScore(const NodeID& nodeID, double delta);
    
    /**
     * @brief Clamp reputation to valid range
     */
    ReputationScore clampReputation(ReputationScore score) const;
    
    /**
     * @brief Log message
     */
    void log(const std::string& message);
    
    /**
     * @brief Get event weight (from paper table)
     */
    EventWeight getEventWeight(ReputationEvent event) const;
    
    // ========================================================================
    // PRIVATE DATA MEMBERS
    // ========================================================================
    
    std::map<NodeID, ReputationRecord> records_;
    LogCallback logCallback_;
};

} // namespace tribft

#endif // VRM_MANAGER_H



