#ifndef TRIBFT_DEFS_H
#define TRIBFT_DEFS_H

#include <string>
#include <vector>
#include <set>
#include <map>
#include <cmath>
#include <omnetpp.h>

using namespace omnetpp;

namespace tribft {

// ============================================================================
// TYPE ALIASES
// ============================================================================

using NodeID = std::string;
using ShardID = int;
using BlockHeight = uint64_t;
using ViewNumber = uint64_t;
using ReputationScore = double;

// ============================================================================
// ENUMS
// ============================================================================

enum class ConsensusPhase {
    IDLE = 0,
    PREPARE = 1,
    PRE_COMMIT = 2,
    COMMIT = 3
};

enum class ReputationEvent {
    SUCCESSFUL_TX = 0,
    FAILED_TX = 1,
    SUCCESSFUL_VOTE = 2,
    FAILED_VOTE = 3,
    TIMEOUT = 4,
    MALICIOUS_BEHAVIOR = 5,
    PROPOSE_VALID_BLOCK = 6,
    PROPOSE_INVALID_BLOCK = 7,
    VOTE_CORRECTLY = 8,
    VOTE_INCORRECTLY = 9,
    SUCCESSFUL_CONSENSUS = 10,
    FAILED_CONSENSUS = 11
};

/**
 * @brief Event weight configuration (from paper table)
 * 
 * For marginal diminishing reward mechanism:
 * - Positive events: alpha = baseWeight / (1 + R_current) (marginal decay)
 * - Negative events: alpha = baseWeight (fixed penalty)
 */
struct EventWeight {
    double baseWeight;      // beta or gamma
    bool useMarginalDecay;  // Whether to use marginal decay
    
    EventWeight(double w = 0.0, bool decay = false) 
        : baseWeight(w), useMarginalDecay(decay) {}
    
    /**
     * @brief Calculate effective weight
     * @param currentReputation Node's current reputation
     * @return Actual applied weight alpha
     */
    double getEffectiveWeight(double currentReputation) const {
        if (useMarginalDecay) {
            // Positive event: marginal decay
            return baseWeight / (1.0 + currentReputation);
        }
        // Negative event: fixed penalty
        return baseWeight;
    }
};

enum class ShardLevel {
    REGIONAL = 0,
    CITY = 1,
    GLOBAL = 2
};

// Note: MessageType is defined in TriBFTMessage.msg and auto-generated

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * @brief Transaction Structure
 */
struct Transaction {
    std::string txID;
    NodeID sender;
    NodeID receiver;
    double value;
    simtime_t timestamp;
    std::string data;
    
    Transaction() : value(0.0), timestamp(0) {}
};

/**
 * @brief Consensus Proposal
 */
struct ConsensusProposal {
    std::string proposalID;
    BlockHeight blockHeight;
    ViewNumber viewNumber;
    NodeID leaderID;
    ShardID shardID;
    simtime_t proposalTime;
    std::vector<Transaction> transactions;
    std::string blockHash;
    
    ConsensusProposal() : blockHeight(0), viewNumber(0), shardID(-1), proposalTime(0) {}
};

/**
 * @brief Vote Information
 * 
 * Note: Renamed from VoteMessage to avoid conflict with generated message class
 */
struct VoteInfo {
    std::string proposalID;
    NodeID voterID;
    ConsensusPhase phase;
    bool approve;
    simtime_t voteTime;
    std::string signature;
    
    VoteInfo() : phase(ConsensusPhase::IDLE), approve(false), voteTime(0) {}
};

/**
 * @brief Quorum Certificate
 */
struct QuorumCertificate {
    std::string proposalID;
    ConsensusPhase phase;
    BlockHeight blockHeight;
    ViewNumber viewNumber;
    std::vector<VoteInfo> votes;
    int totalVotes;
    simtime_t timestamp;
    
    QuorumCertificate() : phase(ConsensusPhase::IDLE), blockHeight(0), 
                          viewNumber(0), totalVotes(0), timestamp(0) {}
    
    bool isValid(int quorumSize) const {
        return totalVotes >= quorumSize;
    }
};

/**
 * @brief Reputation Record
 */
struct ReputationRecord {
    NodeID nodeID;
    
    // Dual Reputation Model
    ReputationScore globalReputation;   // R_global: Cross-domain long-term reputation
    ReputationScore localPerformance;   // R_local: Local instant performance score
    int localInteractionCount;          // N_local: Local interaction count
    
    // Legacy field (backward compatible)
    ReputationScore score;  // Final reputation (dynamically calculated)
    
    // Statistics fields
    int successfulTx;
    int failedTx;
    int validProposals;
    int totalProposals;
    int correctVotes;
    int totalVotes;
    simtime_t lastUpdate;
    std::vector<ReputationEvent> recentEvents;
    
    ReputationRecord() : globalReputation(0.5), localPerformance(0.5), 
                        localInteractionCount(0), score(0.5),
                        successfulTx(0), failedTx(0),
                        validProposals(0), totalProposals(0),
                        correctVotes(0), totalVotes(0), lastUpdate(0) {}
    
    explicit ReputationRecord(const NodeID& id) : nodeID(id), 
                                                   globalReputation(0.5), localPerformance(0.5),
                                                   localInteractionCount(0), score(0.5),
                                                   successfulTx(0), failedTx(0),
                                                   validProposals(0), totalProposals(0),
                                                   correctVotes(0), totalVotes(0), lastUpdate(0) {}
    
    /**
     * @brief Calculate final reputation (dynamic weighting)
     * R_final = w * R_global + (1-w) * R_local
     * where w = exp(-lambda * N_local)
     */
    double getFinalReputation() const {
        // lambda = 0.1 (decay coefficient from paper)
        const double LAMBDA = 0.1;
        double w = std::exp(-LAMBDA * localInteractionCount);
        return w * globalReputation + (1.0 - w) * localPerformance;
    }
    
    /**
     * @brief Check if node is trusted (trusted level)
     * Paper: R_final >= 0.8 is trusted level
     */
    bool isReliable() const {
        return getFinalReputation() >= 0.8;  // Trusted level threshold
    }
    
    /**
     * @brief Check if node is standard level
     * Paper: 0.2 <= R < 0.8
     */
    bool isStandard() const {
        double r = getFinalReputation();
        return r >= 0.2 && r < 0.8;
    }
    
    /**
     * @brief Check if node is candidate level
     * Paper: 0 < R < 0.2
     */
    bool isCandidate() const {
        double r = getFinalReputation();
        return r > 0.0 && r < 0.2;
    }
};

/**
 * @brief Block Structure
 */
struct Block {
    BlockHeight height;
    std::string blockHash;
    std::string previousHash;
    ShardID shardID;
    std::vector<Transaction> transactions;
    QuorumCertificate qc;
    simtime_t timestamp;
    NodeID proposer;
    
    Block() : height(0), shardID(-1), timestamp(0) {}
};

/**
 * @brief Geographic Coordinate
 */
struct GeoCoord {
    double latitude;
    double longitude;
    
    GeoCoord() : latitude(0.0), longitude(0.0) {}
    GeoCoord(double lat, double lon) : latitude(lat), longitude(lon) {}
    
    double distanceTo(const GeoCoord& other) const {
        double dx = latitude - other.latitude;
        double dy = longitude - other.longitude;
        return std::sqrt(dx*dx + dy*dy);
    }
};

/**
 * @brief Shard Information
 */
struct ShardInfo {
    ShardID shardID;
    ShardLevel level;
    GeoCoord centerPoint;
    double radius;
    std::set<NodeID> members;
    NodeID leader;
    simtime_t creationTime;
    simtime_t lastUpdate;
    
    ShardInfo() : shardID(-1), level(ShardLevel::REGIONAL), radius(0.0), creationTime(0), lastUpdate(0) {}
    
    bool contains(const GeoCoord& location) const {
        return centerPoint.distanceTo(location) <= radius;
    }
    
    bool isLeader(const NodeID& nodeID) const {
        return leader == nodeID;
    }
    
    size_t getMemberCount() const {
        return members.size();
    }
};

/**
 * @brief Consensus Metrics
 */
struct ConsensusMetrics {
    int totalProposals;
    int successfulCommits;
    int failedConsensus;
    double avgLatency;
    double minLatency;
    double maxLatency;
    double totalLatency;
    double throughput;
    int totalTransactions;
    
    ConsensusMetrics() : totalProposals(0), successfulCommits(0), 
                         failedConsensus(0), avgLatency(0.0), 
                         minLatency(999999.0), maxLatency(0.0),
                         totalLatency(0.0), throughput(0.0), totalTransactions(0) {}
};

/**
 * @brief Shard Metrics
 */
struct ShardMetrics {
    int totalShards;
    double avgShardSize;
    int splitCount;
    int mergeCount;
    double loadBalance;
    
    ShardMetrics() : totalShards(0), avgShardSize(0.0), 
                     splitCount(0), mergeCount(0), loadBalance(0.0) {}
};

// ============================================================================
// CONSTANTS
// ============================================================================

namespace Constants {
    // Consensus Parameters
    constexpr double QUORUM_RATIO = 2.0 / 3.0;  // > 2/3 for Byzantine Fault Tolerance
    constexpr int MIN_QUORUM_SIZE = 2;           // Minimum quorum size
    constexpr double CONSENSUS_TIMEOUT_SEC = 5.0; // Consensus timeout (seconds)
    
    // Shard Parameters (optimized: smaller shard radius for better multi-hop efficiency)
    constexpr double REGIONAL_SHARD_RADIUS = 3000.0;  // meters (3km radius, balance coverage and communication)
    constexpr int MIN_SHARD_SIZE = 50;   // Minimum shard size (for smaller shards)
    constexpr int MAX_SHARD_SIZE = 250;  // Maximum shard size (for smaller shards)
    constexpr double SPLIT_THRESHOLD = 0.8;  // Split when > 80% full
    constexpr double MERGE_THRESHOLD = 0.3;  // Merge when < 30% full
    
    // Reputation Parameters
    constexpr double INITIAL_REPUTATION = 0.5;
    constexpr double MIN_REPUTATION = 0.0;
    constexpr double MAX_REPUTATION = 1.0;
    constexpr double REPUTATION_DECAY_RATE = 0.01;
    constexpr double REPUTATION_SUCCESS_REWARD = 0.05;
    constexpr double REPUTATION_FAILURE_PENALTY = 0.1;
    constexpr double REWARD_VALID_PROPOSAL = 0.03;
    constexpr double PENALTY_INVALID_PROPOSAL = 0.08;
    constexpr double REWARD_CORRECT_VOTE = 0.02;
    constexpr double PENALTY_INCORRECT_VOTE = 0.05;
    
    // Network Parameters
    constexpr int MAX_TRANSACTION_POOL_SIZE = 1000;
    constexpr int DEFAULT_BATCH_SIZE = 100;
    constexpr double DEFAULT_BLOCK_INTERVAL_SEC = 0.5;  // seconds
}

} // namespace tribft

#endif // TRIBFT_DEFS_H

