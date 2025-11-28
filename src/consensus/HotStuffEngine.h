#ifndef HOTSTUFF_ENGINE_H
#define HOTSTUFF_ENGINE_H

#include <queue>
#include <functional>
#include "../common/TriBFTDefs.h"

namespace tribft {

/**
 * @brief HotStuff Consensus Engine (Three-Phase BFT Consensus)
 * 
 * Implements the HotStuff consensus protocol with three phases:
 * 1. PREPARE: Leader proposes a block
 * 2. PRE-COMMIT: Nodes vote on the proposal
 * 3. COMMIT: Nodes commit the block
 * 
 * Design Principles:
 * - SOLID: Single responsibility for consensus logic
 * - KISS: Clear three-phase state machine
 * - YAGNI: Only implement core HotStuff features
 * 
 * NOTE: This is a plain C++ class, NOT an OMNeT++ module.
 * Therefore, it cannot use EV_INFO or other OMNeT++ macros.
 * Logging is delegated to the caller (TriBFTApp).
 */
class HotStuffEngine {
public:
    // Callback types for delegation (Dependency Inversion)
    using ProposalCallback = std::function<void(const ConsensusProposal&)>;
    using VoteCallback = std::function<void(const VoteInfo&)>;
    using CommitCallback = std::function<void(const Block&)>;
    using LogCallback = std::function<void(const std::string&)>;
    using PhaseAdvanceCallback = std::function<void(const std::string&, ConsensusPhase, ConsensusPhase)>; // proposalID, fromPhase, toPhase
    
    HotStuffEngine();
    ~HotStuffEngine() = default;
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    /**
     * @brief Initialize the consensus engine
     */
    void initialize(const NodeID& nodeID, ShardID shardID);
    
    /**
     * @brief Set shard size for quorum calculation
     */
    void setShardSize(int size);
    
    /**
     * @brief Set callbacks for external communication
     */
    void setProposalCallback(ProposalCallback callback);
    void setVoteCallback(VoteCallback callback);
    void setCommitCallback(CommitCallback callback);
    void setLogCallback(LogCallback callback);
    void setPhaseAdvanceCallback(PhaseAdvanceCallback callback);
    
    // ========================================================================
    // CONSENSUS INTERFACE (Leader)
    // ========================================================================
    
    /**
     * @brief Propose a new block (called by leader)
     * @return true if proposal initiated successfully
     */
    bool proposeBlock(const std::vector<Transaction>& transactions);
    
    /**
     * @brief Check if this node is ready to propose
     */
    bool canPropose() const;
    
    // ========================================================================
    // CONSENSUS INTERFACE (Replica)
    // ========================================================================
    
    /**
     * @brief Handle incoming proposal (Phase 1: PREPARE)
     */
    void handleProposal(const ConsensusProposal& proposal);
    
    /**
     * @brief Handle incoming vote (Phase 2 & 3: PRE-COMMIT, COMMIT)
     */
    void handleVote(const VoteInfo& vote);
    
    /**
     * @brief Handle phase advance message from leader (for follower nodes)
     */
    void handlePhaseAdvance(const std::string& proposalID, ConsensusPhase toPhase);
    
    /**
     * @brief Handle timeout event
     */
    void handleTimeout();
    
    // ========================================================================
    // STATE QUERIES
    // ========================================================================
    
    ConsensusPhase getCurrentPhase() const { return currentPhase_; }
    ViewNumber getCurrentView() const { return currentView_; }
    BlockHeight getCurrentHeight() const { return currentHeight_; }
    
    // Sync to specified height (lightweight sync)
    void syncToHeight(BlockHeight newHeight);
    
    const ConsensusProposal* getCurrentProposal() const;
    const QuorumCertificate* getHighestQC() const;
    
    bool isInProgress() const;
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    const ConsensusMetrics& getMetrics() const { return metrics_; }
    
private:
    // ========================================================================
    // PRIVATE CONSENSUS LOGIC
    // ========================================================================
    
    /**
     * @brief Validate proposal
     */
    bool validateProposal(const ConsensusProposal& proposal);
    
    /**
     * @brief Create and send vote
     */
    void sendVote(const ConsensusProposal& proposal, ConsensusPhase phase, bool approve);
    
    /**
     * @brief Check if we have quorum for current phase
     */
    bool hasQuorum(const std::string& proposalID, ConsensusPhase phase);
    
    /**
     * @brief Advance to next phase
     */
    void advancePhase();
    
    /**
     * @brief Commit the block
     */
    void commitBlock();
    
    /**
     * @brief Generate unique proposal ID
     */
    std::string generateProposalID();
    
    /**
     * @brief Calculate quorum size (> 2/3)
     */
    int getQuorumSize() const;
    
    /**
     * @brief Create Quorum Certificate from votes
     */
    QuorumCertificate createQC(const std::string& proposalID, ConsensusPhase phase);
    
    /**
     * @brief Reset consensus state for new round
     */
    void resetConsensusState();
    
    /**
     * @brief Log message (delegates to callback)
     */
    void log(const std::string& message);
    
    // ========================================================================
    // PRIVATE DATA MEMBERS
    // ========================================================================
    
    // Node identity
    NodeID nodeID_;
    ShardID shardID_;
    int shardSize_;
    
    // Consensus state
    ConsensusPhase currentPhase_;
    ViewNumber currentView_;
    BlockHeight currentHeight_;
    std::string previousBlockHash_;
    
    // Current proposal being processed
    ConsensusProposal currentProposal_;
    bool hasActiveProposal_;
    
    // Vote collection (proposalID -> phase -> votes)
    std::map<std::string, std::map<ConsensusPhase, std::vector<VoteInfo>>> voteStore_;
    
    // Quorum Certificates
    QuorumCertificate highestQC_;
    std::map<ConsensusPhase, QuorumCertificate> phaseQCs_;
    
    // Committed blocks
    std::vector<Block> committedBlocks_;
    
    // Callbacks
    ProposalCallback proposalCallback_;
    VoteCallback voteCallback_;
    CommitCallback commitCallback_;
    LogCallback logCallback_;
    PhaseAdvanceCallback phaseAdvanceCallback_;
    
    // Metrics
    ConsensusMetrics metrics_;
    simtime_t consensusStartTime_;
};

} // namespace tribft

#endif // HOTSTUFF_ENGINE_H

