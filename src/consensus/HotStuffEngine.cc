#include "HotStuffEngine.h"
#include <sstream>
#include <iomanip>

namespace tribft {

HotStuffEngine::HotStuffEngine()
    : shardSize_(0)
    , currentPhase_(ConsensusPhase::IDLE)
    , currentView_(0)
    , currentHeight_(0)
    , hasActiveProposal_(false)
    , consensusStartTime_(0)
{
}

void HotStuffEngine::initialize(const NodeID& nodeID, ShardID shardID) {
    nodeID_ = nodeID;
    shardID_ = shardID;
    currentPhase_ = ConsensusPhase::IDLE;
    currentView_ = 0;
    currentHeight_ = 0;
    hasActiveProposal_ = false;
    
    log("HotStuff consensus engine initialized for node " + nodeID_);
}

void HotStuffEngine::setShardSize(int size) {
    shardSize_ = size;
    log("Shard size set to " + std::to_string(size));
}

void HotStuffEngine::setProposalCallback(ProposalCallback callback) {
    proposalCallback_ = callback;
}

void HotStuffEngine::setVoteCallback(VoteCallback callback) {
    voteCallback_ = callback;
}

void HotStuffEngine::setCommitCallback(CommitCallback callback) {
    commitCallback_ = callback;
}

void HotStuffEngine::setLogCallback(LogCallback callback) {
    logCallback_ = callback;
}

void HotStuffEngine::setPhaseAdvanceCallback(PhaseAdvanceCallback callback) {
    phaseAdvanceCallback_ = callback;
}

// ============================================================================
// LEADER INTERFACE
// ============================================================================

bool HotStuffEngine::proposeBlock(const std::vector<Transaction>& transactions) {
    if (!canPropose()) {
        log("Cannot propose: consensus already in progress");
        return false;
    }
    
    if (transactions.empty()) {
        log("Cannot propose: no transactions");
        return false;
    }
    
    // Create proposal
    ConsensusProposal proposal;
    proposal.proposalID = generateProposalID();
    proposal.blockHeight = currentHeight_ + 1;
    proposal.viewNumber = currentView_;
    proposal.leaderID = nodeID_;
    proposal.shardID = shardID_;
    proposal.proposalTime = simTime();
    proposal.transactions = transactions;
    
    // Calculate block hash
    std::stringstream ss;
    ss << proposal.blockHeight << "_" << previousBlockHash_ << "_" << proposal.proposalTime;
    proposal.blockHash = ss.str();
    
    // Set as current proposal
    currentProposal_ = proposal;
    hasActiveProposal_ = true;
    currentPhase_ = ConsensusPhase::PREPARE;
    consensusStartTime_ = simTime();
    
    metrics_.totalProposals++;
    
    log("Proposed block " + std::to_string(proposal.blockHeight) + 
        " with " + std::to_string(transactions.size()) + " transactions");
    
    // Broadcast proposal to shard members
    if (proposalCallback_) {
        proposalCallback_(proposal);
    }
    
    // Self-vote
    sendVote(proposal, ConsensusPhase::PREPARE, true);
    
    return true;
}

bool HotStuffEngine::canPropose() const {
    return currentPhase_ == ConsensusPhase::IDLE && !hasActiveProposal_;
}

// ============================================================================
// REPLICA INTERFACE
// ============================================================================

void HotStuffEngine::handleProposal(const ConsensusProposal& proposal) {
    std::cout << "  [ENGINE] " << nodeID_ << " validating proposal " << proposal.proposalID << std::endl;
    std::cout << "    My shard: " << shardID_ << ", Proposal shard: " << proposal.shardID << std::endl;
    std::cout << "    My height: " << currentHeight_ << ", Proposal height: " << proposal.blockHeight << std::endl;
    
    // Validate proposal
    if (!validateProposal(proposal)) {
        std::cout << "  [ENGINE] Validation FAILED!" << std::endl;
        sendVote(proposal, ConsensusPhase::PREPARE, false);
        return;
    }
    
    std::cout << "  [ENGINE] Validation SUCCESS!" << std::endl;
    
    // Accept proposal
    currentProposal_ = proposal;
    hasActiveProposal_ = true;
    currentPhase_ = ConsensusPhase::PREPARE;
    consensusStartTime_ = simTime();
    
    // Vote for proposal
    sendVote(proposal, ConsensusPhase::PREPARE, true);
}

void HotStuffEngine::handleVote(const VoteInfo& vote) {
    if (!hasActiveProposal_ || vote.proposalID != currentProposal_.proposalID) {
        log("Vote for unknown proposal ignored");
        return;
    }
    
    // Store vote
    voteStore_[vote.proposalID][vote.phase].push_back(vote);
    
    int voteCount = voteStore_[vote.proposalID][vote.phase].size();
    std::cout << "  [ENGINE-VOTE] " << nodeID_ << " got vote from " << vote.voterID 
              << " phase=" << static_cast<int>(vote.phase)
              << " current_phase=" << static_cast<int>(currentPhase_)
              << " (" << (vote.approve ? "YES" : "NO") << ")"
              << " total_votes=" << voteCount << std::endl;
    
    log("Received vote from " + vote.voterID + 
        " for phase " + std::to_string(static_cast<int>(vote.phase)) +
        " (" + (vote.approve ? "approve" : "reject") + ")");
    
    // Fix: Check quorum for the vote's phase, not just current phase
    // This handles late-arriving votes (proposer may have advanced to next phase)
    
    // If vote is for current phase, check if quorum is reached
    if (vote.phase == currentPhase_) {
        if (hasQuorum(vote.proposalID, currentPhase_)) {
            std::cout << "  [ENGINE-QUORUM] " << nodeID_ << " phase " << static_cast<int>(currentPhase_) 
                      << " reached quorum" << std::endl;
            log("Quorum reached for phase " + std::to_string(static_cast<int>(currentPhase_)));
            advancePhase();
        }
    }
    // If vote is for previous phase, check if we need to advance
    else if (vote.phase < currentPhase_) {
        std::cout << "  [ENGINE-LATE] " << nodeID_ << " got late vote for phase " 
                  << static_cast<int>(vote.phase) << " (already in phase " 
                  << static_cast<int>(currentPhase_) << ")" << std::endl;
        // Late vote, check if previous phase now has quorum
        // If so, ensure we advance to the correct phase
        for (int p = static_cast<int>(vote.phase); p < static_cast<int>(currentPhase_); ++p) {
            ConsensusPhase phase = static_cast<ConsensusPhase>(p);
            if (hasQuorum(vote.proposalID, phase)) {
                std::cout << "    [ENGINE-LATE-QUORUM] Phase " << p << " has quorum" << std::endl;
            }
        }
    }
    // If vote is for future phase, ignore
    else {
        std::cout << "  [ENGINE-FUTURE] " << nodeID_ << " got future vote for phase " 
                  << static_cast<int>(vote.phase) << " (current phase " 
                  << static_cast<int>(currentPhase_) << ")" << std::endl;
    }
}

void HotStuffEngine::handleTimeout() {
    if (hasActiveProposal_) {
        log("Consensus timeout - resetting state");
        metrics_.failedConsensus++;
        resetConsensusState();
    }
}

// ============================================================================
// STATE QUERIES
// ============================================================================

const ConsensusProposal* HotStuffEngine::getCurrentProposal() const {
    if (hasActiveProposal_) {
        return &currentProposal_;
    }
    return nullptr;
}

const QuorumCertificate* HotStuffEngine::getHighestQC() const {
    if (!highestQC_.proposalID.empty()) {
        return &highestQC_;
    }
    return nullptr;
}

bool HotStuffEngine::isInProgress() const {
    return hasActiveProposal_ && currentPhase_ != ConsensusPhase::IDLE;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool HotStuffEngine::validateProposal(const ConsensusProposal& proposal) {
    // Basic validation
    if (proposal.proposalID.empty() || proposal.blockHash.empty()) {
        log("Invalid proposal: empty ID or hash");
        return false;
    }
    
    // Check height
    if (proposal.blockHeight != currentHeight_ + 1) {
        std::cout << "[HEIGHT-MISMATCH] " << nodeID_ << " currentHeight=" << currentHeight_ 
                  << " expected=" << (currentHeight_ + 1) 
                  << " proposal=" << proposal.blockHeight << std::endl;
        log("Invalid height: expected " + std::to_string(currentHeight_ + 1) + 
            ", got " + std::to_string(proposal.blockHeight));
        return false;
    }
    
    // Check view
    if (proposal.viewNumber < currentView_) {
        log("Stale view number");
        return false;
    }
    
    // Check shard - TEMPORARILY DISABLED FOR TESTING
    // if (proposal.shardID != shardID_) {
    //     log("Wrong shard");
    //     return false;
    // }
    
    // Validate transactions - check for empty transactions
    if (proposal.transactions.empty()) {
        log("Proposal has no transactions");
        return false;
    }
    
    for (const auto& tx : proposal.transactions) {
        if (tx.txID.empty() || tx.sender.empty()) {
            log("Invalid transaction in proposal: empty ID or sender");
            return false;
        }
    }
    
    return true;
}

void HotStuffEngine::sendVote(const ConsensusProposal& proposal, ConsensusPhase phase, bool approve) {
    VoteInfo vote;
    vote.proposalID = proposal.proposalID;
    vote.voterID = nodeID_;
    vote.phase = phase;
    vote.approve = approve;
    vote.voteTime = simTime();
    vote.signature = nodeID_ + "_" + proposal.proposalID;  // Simplified signature
    
    // No longer add directly to voteStore_, handle uniformly via handleVote
    // This ensures vote counting consistency
    
    // Send via callback
    if (voteCallback_) {
        voteCallback_(vote);
    }
}

bool HotStuffEngine::hasQuorum(const std::string& proposalID, ConsensusPhase phase) {
    // TODO: Core implementation hidden - will be released after project completion
    return false;
}

void HotStuffEngine::advancePhase() {
    // TODO: Core implementation hidden - will be released after project completion
}

void HotStuffEngine::handlePhaseAdvance(const std::string& proposalID, ConsensusPhase toPhase) {
    // Verify this is for our current proposal
    if (proposalID != currentProposal_.proposalID) {
        std::cout << "  [PHASE-ADVANCE] Ignoring advance for different proposal" << std::endl;
        return;
    }
    
    // Verify phase transition is valid
    ConsensusPhase expectedNext = ConsensusPhase::IDLE;
    switch (currentPhase_) {
        case ConsensusPhase::PREPARE:
            expectedNext = ConsensusPhase::PRE_COMMIT;
            break;
        case ConsensusPhase::PRE_COMMIT:
            expectedNext = ConsensusPhase::COMMIT;
            break;
        default:
            std::cout << "  [PHASE-ADVANCE] Invalid current phase for advance" << std::endl;
            return;
    }
    
    if (toPhase != expectedNext) {
        std::cout << "  [PHASE-ADVANCE] Unexpected phase transition: " 
                  << (int)currentPhase_ << " -> " << (int)toPhase 
                  << " (expected " << (int)expectedNext << ")" << std::endl;
        return;
    }
    
    std::cout << "  [PHASE-ADVANCE] " << nodeID_ << " advancing to phase " << (int)toPhase << std::endl;
    
    // Advance phase
    currentPhase_ = toPhase;
    log("Follower advanced to phase " + std::to_string((int)toPhase));
    
    // Send vote for new phase
    sendVote(currentProposal_, toPhase, true);
}

void HotStuffEngine::commitBlock() {
    // TODO: Core implementation hidden - will be released after project completion
}

std::string HotStuffEngine::generateProposalID() {
    std::stringstream ss;
    ss << nodeID_ << "_" << currentView_ << "_" << (currentHeight_ + 1) << "_" 
       << std::setprecision(6) << simTime();
    return ss.str();
}

int HotStuffEngine::getQuorumSize() const {
    // Use fixed small quorum (suitable for dynamic IoV scenarios)
    // Fixed quorum: 2 nodes (minimum viable: leader + 1 follower)
    // Ignore shardSize_ to avoid quorum inconsistency from dynamic node joins
    return 2;
}

QuorumCertificate HotStuffEngine::createQC(const std::string& proposalID, ConsensusPhase phase) {
    QuorumCertificate qc;
    qc.proposalID = proposalID;
    qc.phase = phase;
    qc.blockHeight = currentProposal_.blockHeight;
    qc.viewNumber = currentView_;
    
    auto propIt = voteStore_.find(proposalID);
    if (propIt != voteStore_.end()) {
        auto phaseIt = propIt->second.find(phase);
        if (phaseIt != propIt->second.end()) {
            qc.votes = phaseIt->second;
        }
    }
    
    return qc;
}

void HotStuffEngine::resetConsensusState() {
    currentPhase_ = ConsensusPhase::IDLE;
    hasActiveProposal_ = false;
    voteStore_.clear();
    phaseQCs_.clear();
}

void HotStuffEngine::syncToHeight(BlockHeight newHeight) {
    if (newHeight > currentHeight_) {
        std::cout << "[SYNC-ENGINE] " << nodeID_ << " updating height from " 
                  << currentHeight_ << " to " << newHeight << std::endl;
        currentHeight_ = newHeight;
        // In production system, this should:
        // 1. Request and verify missing blocks
        // 2. Apply state transitions
        // 3. Update Merkle tree
        // Simplified: directly update height
    }
}

void HotStuffEngine::log(const std::string& message) {
    if (logCallback_) {
        logCallback_(message);
    }
}

} // namespace tribft

