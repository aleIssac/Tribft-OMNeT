//
// TriBFT Message Definitions
// Byzantine Fault Tolerance Consensus Messages for IoV
//

import veins.modules.messages.BaseFrame1609_4;

namespace tribft;

// ============================================================================
// MESSAGE TYPES
// ============================================================================

enum MessageType {
    // Transaction messages
    MT_TRANSACTION = 0;  // Transaction broadcast to Leader
    
    // Consensus messages (HotStuff three-phase)
    MT_PROPOSAL = 1;
    MT_VOTE_PREPARE = 2;
    MT_VOTE_PRE_COMMIT = 3;
    MT_VOTE_COMMIT = 4;
    MT_DECIDE = 5;
    
    // Shard management messages
    MT_SHARD_JOIN_REQUEST = 10;
    MT_SHARD_JOIN_RESPONSE = 11;
    MT_SHARD_UPDATE = 12;
    MT_SHARD_LEAVE = 13;
    MT_LEADER_ELECTION = 14;
    
    // Reputation messages
    MT_REPUTATION_UPDATE = 20;
    MT_REPUTATION_QUERY = 21;
    MT_REPUTATION_RESPONSE = 22;
    
    // Cross-shard messages
    MT_CROSS_SHARD_TX = 30;
    MT_CROSS_SHARD_COMMIT = 31;
    
    // System messages
    MT_HEARTBEAT = 40;
    MT_VIEW_CHANGE = 41;
    MT_PHASE_ADVANCE = 42;  // Leader coordinates phase transitions
};

enum ConsensusPhaseType {
    PHASE_IDLE = 0;
    PHASE_PREPARE = 1;
    PHASE_PRE_COMMIT = 2;
    PHASE_COMMIT = 3;
    PHASE_DECIDE = 4;
};

enum ShardLevelType {
    SHARD_REGIONAL = 0;
    SHARD_CITY = 1;
    SHARD_GLOBAL = 2;
};

// ============================================================================
// BASE MESSAGE
// ============================================================================

packet TriBFTMessage extends veins::BaseFrame1609_4 {
    int messageType @enum(MessageType);
    string senderID;
    int shardID = -1;
    int viewNumber = 0;
    simtime_t timestamp;
}

// ============================================================================
// TRANSACTION MESSAGES
// ============================================================================

packet TransactionMessage extends TriBFTMessage {
    messageType = MT_TRANSACTION;
    string txID;
    string txData;  // Serialized transaction data OR serialized consensus message
    int hopCount = 0;  // Multi-hop forwarding: track number of hops
    double senderDistanceToLeader = -1.0;  // Smart forwarding: sender's distance to Leader
    int targetShardId = -1;  // Smart forwarding: target shard ID for filtering
    
    // 🔧 WORKAROUND: Only TransactionMessage can be transmitted via Veins
    // Use this field to identify the actual message type (PROPOSAL, VOTE, etc.)
    int actualMessageType = -1;  // -1=real TX, others=disguised consensus messages
}

// ============================================================================
// CONSENSUS MESSAGES
// ============================================================================

// 🧪 MINIMAL TEST MESSAGE
packet TestMinimalMessage extends TriBFTMessage {
    messageType = MT_TRANSACTION;
    string testField = "TEST";
}

packet ProposalMessage extends TriBFTMessage {
    messageType = MT_TRANSACTION;  // 🧪 TEST: Use TX type to see if type=1 is the problem
    string proposalID;
    string blockHash;
    int blockHeight;
    string leaderID;
    int txCount;
    // 🔧 txData REMOVED: too large for V2V broadcast, causes message loss
    // Consensus members only need hash for voting, not full transaction data
}

packet VoteMessage extends TriBFTMessage {
    string proposalID;
    int phase @enum(ConsensusPhaseType);
    bool approve;
    string signature;
}

packet DecideMessage extends TriBFTMessage {
    messageType = MT_DECIDE;
    string proposalID;
    string blockHash;
    int blockHeight;
    bool committed;
}

// ============================================================================
// SHARD MANAGEMENT MESSAGES
// ============================================================================

packet ShardJoinRequest extends TriBFTMessage {
    messageType = MT_SHARD_JOIN_REQUEST;
    double latitude;
    double longitude;
    double reputationScore;
}

packet ShardJoinResponse extends TriBFTMessage {
    messageType = MT_SHARD_JOIN_RESPONSE;
    int assignedShardID;
    bool accepted;
    string leaderID;
    int memberCount;
}

packet ShardUpdateMessage extends TriBFTMessage {
    messageType = MT_SHARD_UPDATE;
    int shardLevel @enum(ShardLevelType);
    string leaderID;
    int memberCount;
    double centerLat;
    double centerLon;
    double radius;
}

packet ShardLeaveMessage extends TriBFTMessage {
    messageType = MT_SHARD_LEAVE;
    string leavingNodeID;
    string reason;
}

packet LeaderElectionMessage extends TriBFTMessage {
    messageType = MT_LEADER_ELECTION;
    string candidateID;
    double reputationScore;
    int voteCount;
}

// ============================================================================
// REPUTATION MESSAGES
// ============================================================================

packet ReputationUpdateMessage extends TriBFTMessage {
    messageType = MT_REPUTATION_UPDATE;
    string targetNodeID;
    double newScore;
    int eventType;  // ReputationEvent
    string reason;
}

packet ReputationQueryMessage extends TriBFTMessage {
    messageType = MT_REPUTATION_QUERY;
    string targetNodeID;
}

packet ReputationResponseMessage extends TriBFTMessage {
    messageType = MT_REPUTATION_RESPONSE;
    string targetNodeID;
    double reputationScore;
    int totalProposals;
    int validProposals;
    int totalVotes;
    int correctVotes;
}

// ============================================================================
// CROSS-SHARD MESSAGES
// ============================================================================

packet CrossShardTxMessage extends TriBFTMessage {
    messageType = MT_CROSS_SHARD_TX;
    string txID;
    int sourceShardID;
    int targetShardID;
    string txData;
}

packet CrossShardCommitMessage extends TriBFTMessage {
    messageType = MT_CROSS_SHARD_COMMIT;
    string txID;
    bool committed;
}

// ============================================================================
// SYSTEM MESSAGES
// ============================================================================

packet HeartbeatMessage extends TriBFTMessage {
    messageType = MT_HEARTBEAT;
    double currentLoad;
    int activeTxCount;
}

packet ViewChangeMessage extends TriBFTMessage {
    messageType = MT_VIEW_CHANGE;
    int newViewNumber;
    string newLeaderID;
    string reason;
}

packet PhaseAdvanceMessage extends TriBFTMessage {
    messageType = MT_PHASE_ADVANCE;
    string proposalID;
    int fromPhase @enum(ConsensusPhaseType);
    int toPhase @enum(ConsensusPhaseType);
}

