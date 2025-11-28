#include "TriBFTApp.h"
#include <iomanip>  // For std::fixed, std::setprecision
#include <cmath>    // For std::sqrt

namespace tribft {

Define_Module(TriBFTApp);

// ============================================================================
// INITIALIZATION
// ============================================================================

void TriBFTApp::initialize(int stage) {
    std::cout << "[INIT] " << getParentModule()->getFullName() << " stage=" << stage << " START" << std::endl;
    
    DemoBaseApplLayer::initialize(stage);
    
    std::cout << "[INIT] " << getParentModule()->getFullName() << " stage=" << stage << " after parent init" << std::endl;
    
    if (stage == 0) {
        // Read parameters
        blockInterval_ = par("blockInterval");
        batchSize_ = par("batchSize");
        consensusTimeout_ = par("consensusTimeout");
        vrmEnabled_ = par("vrmEnabled");
        initialReputation_ = par("initialReputation");
        
        // 🆕 读取自动交易生成参数
        autoGenerateTx_ = par("autoGenerateTx").boolValue();
        txGenerationInterval_ = par("txGenerationInterval");
        
        // 🆕 多跳转发参数
        enableMultiHop_ = par("enableMultiHop").boolValue();
        maxHops_ = par("maxHops");
        
        std::cout << "[INIT] " << getParentModule()->getFullName() << " stage=" << stage << " params read" << std::endl;
        std::cout << "[TX-GEN] autoGenerateTx=" << autoGenerateTx_ << " interval=" << txGenerationInterval_ << std::endl;
        std::cout << "[MULTI-HOP] enabled=" << enableMultiHop_ << " maxHops=" << maxHops_ << std::endl;
        
        // Register signals
        blockCommittedSignal_ = registerSignal("blockCommitted");
        consensusLatencySignal_ = registerSignal("consensusLatency");
        reputationSignal_ = registerSignal("reputation");
        throughputSignal_ = registerSignal("throughput");
        shardSizeSignal_ = registerSignal("shardSize");
        
        // Initialize state
        nodeID_ = getNodeID();
        currentShardID_ = -1;
        isLeaderNode_ = false;
        isInitialized_ = false;
        txCounter_ = 0;
        
        // 🆕 共识群组管理初始�?        nodeRole_ = NodeRole::ORDINARY;
        lastElectionEpoch_ = -1;
        committedBlockCount_ = 0;
        epochBlocks_ = 10;  // �?0个区块重新选举
        
        // Create timers
        consensusTimer_ = new cMessage("consensusTimer");
        shardMaintenanceTimer_ = new cMessage("shardMaintenanceTimer");
        reputationDecayTimer_ = new cMessage("reputationDecayTimer");
        heartbeatTimer_ = new cMessage("heartbeatTimer");
        txGenerationTimer_ = new cMessage("txGenerationTimer");  // 🆕 交易生成定时�?        
        EV_INFO << "[TriBFT] Node " << nodeID_ << " initialized (stage 0)" << endl;
    }
    else if (stage == 1) {
        // Initialize components
        std::cout << "[INIT] " << getParentModule()->getFullName() << " stage=" << stage << " before initializeShard" << std::endl;
        initializeShard();
        std::cout << "[INIT] " << getParentModule()->getFullName() << " stage=" << stage << " after initializeShard" << std::endl;
        initializeConsensus();
        std::cout << "[INIT] " << getParentModule()->getFullName() << " stage=" << stage << " after initializeConsensus" << std::endl;
        initializeReputation();
        std::cout << "[INIT] " << getParentModule()->getFullName() << " stage=" << stage << " after initializeReputation" << std::endl;
        initializeTimers();
        std::cout << "[INIT] " << getParentModule()->getFullName() << " stage=" << stage << " after initializeTimers" << std::endl;
        
        isInitialized_ = true;
        EV_INFO << "[TriBFT] Node " << nodeID_ << " fully initialized" << endl;
        std::cout << "[INIT] " << getParentModule()->getFullName() << " stage=" << stage << " COMPLETE" << std::endl;
    }
    
    std::cout << "[INIT] " << getParentModule()->getFullName() << " stage=" << stage << " END" << std::endl;
}

void TriBFTApp::finish() {
    DemoBaseApplLayer::finish();
    
    // Cancel timers
    cancelAndDelete(consensusTimer_);
    cancelAndDelete(shardMaintenanceTimer_);
    cancelAndDelete(reputationDecayTimer_);
    cancelAndDelete(heartbeatTimer_);
    cancelAndDelete(txGenerationTimer_);  // 🆕 清理交易生成定时�?    
    // Record final statistics
    recordStatistics();
    
    EV_INFO << "[TriBFT] Node " << nodeID_ << " finished" << endl;
}

void TriBFTApp::initializeShard() {
    std::cout << "[INIT-SHARD] " << nodeID_ << " start" << std::endl;
    
    // 🔧 Get global shared shard manager (all nodes use the same instance)
    shardManager_ = RegionalShardManager::getGlobalInstance();
    
    std::cout << "[INIT-SHARD] " << nodeID_ << " got shard manager" << std::endl;
    
    // 🔧 Initialize on first access (thread-safe within OMNeT++ single-threaded execution)
    static bool initialized = false;
    if (!initialized) {
        shardManager_->initialize(
            Constants::REGIONAL_SHARD_RADIUS,
            Constants::MIN_SHARD_SIZE,
            Constants::MAX_SHARD_SIZE
        );
        initialized = true;
        EV_INFO << "🌍 [GLOBAL SHARD MANAGER] Initialized:" << endl;
        EV_INFO << "  - Radius: " << Constants::REGIONAL_SHARD_RADIUS << "m" << endl;
        EV_INFO << "  - Min Size: " << Constants::MIN_SHARD_SIZE << endl;
        EV_INFO << "  - Max Size: " << Constants::MAX_SHARD_SIZE << endl;
        std::cout << "[INIT-SHARD] Global shard manager initialized" << std::endl;
    }
    
    // Join shard
    std::cout << "[INIT-SHARD] " << nodeID_ << " before getCurrentLocation" << std::endl;
    GeoCoord location = getCurrentLocation();
    std::cout << "[INIT-SHARD] " << nodeID_ << " location=(" << location.latitude << "," << location.longitude << ")" << std::endl;
    currentShardID_ = shardManager_->addNode(nodeID_, location, initialReputation_);
    
    const ShardInfo* shard = shardManager_->getShardInfo(currentShardID_);
    int memberCount = shard ? shard->getMemberCount() : 0;
    
    EV_INFO << "🔗 [SHARD JOIN] Node " << nodeID_ << ":" << endl;
    EV_INFO << "  - Shard ID: " << currentShardID_ << endl;
    EV_INFO << "  - Position: (" << location.latitude << ", " << location.longitude << ")" << endl;
    EV_INFO << "  - Shard Size: " << memberCount << " members" << endl;
}

void TriBFTApp::initializeConsensus() {
    consensusEngine_ = std::make_unique<HotStuffEngine>();
    consensusEngine_->initialize(nodeID_, currentShardID_);
    
    // Set callbacks
    consensusEngine_->setProposalCallback([this](const ConsensusProposal& proposal) {
        this->onProposalGenerated(proposal);
    });
    
    consensusEngine_->setVoteCallback([this](const tribft::VoteInfo& vote) {
        this->onVoteGenerated(vote);
    });
    
    consensusEngine_->setCommitCallback([this](const Block& block) {
        this->onBlockCommitted(block);
    });
    
    consensusEngine_->setLogCallback([this](const std::string& msg) {
        this->onConsensusLog(msg);
    });
    
    consensusEngine_->setPhaseAdvanceCallback([this](const std::string& proposalID, ConsensusPhase fromPhase, ConsensusPhase toPhase) {
        this->sendPhaseAdvance(proposalID, fromPhase, toPhase);
    });
    
    // Update shard size
    const ShardInfo* shard = shardManager_->getShardInfo(currentShardID_);
    if (shard) {
        consensusEngine_->setShardSize(shard->getMemberCount());
        isLeaderNode_ = shardManager_->isShardLeader(nodeID_, currentShardID_);
    }
    
    EV_INFO << "[TriBFT] Consensus engine initialized (Leader: " 
            << (isLeaderNode_ ? "YES" : "NO") << ")" << endl;
}

void TriBFTApp::initializeReputation() {
    reputationManager_ = std::make_unique<VRMManager>();
    reputationManager_->initialize();
    
    reputationManager_->setLogCallback([this](const std::string& msg) {
        EV_DEBUG << "[VRM] " << msg << endl;
    });
    
    // Register self
    reputationManager_->registerNode(nodeID_, initialReputation_);
    
    // Register other shard members
    const ShardInfo* shard = shardManager_->getShardInfo(currentShardID_);
    if (shard) {
        for (const NodeID& member : shard->members) {
            if (member != nodeID_) {
                reputationManager_->registerNode(member, Constants::INITIAL_REPUTATION);
            }
        }
    }
    
    EV_INFO << "[TriBFT] Reputation system initialized" << endl;
}

void TriBFTApp::initializeTimers() {
    // Check if this node is the leader of its shard
    isLeaderNode_ = shardManager_->isShardLeader(nodeID_, currentShardID_);
    
    // 🔍 强制输出到控制台（调试用�?    std::cout << "========================================" << std::endl;
    std::cout << "[DEBUG] Node " << nodeID_ << " Timer Init:" << std::endl;
    std::cout << "  - Shard: " << currentShardID_ << std::endl;
    std::cout << "  - Leader: " << (isLeaderNode_ ? "YES" : "NO") << std::endl;
    std::cout << "  - Interval: " << blockInterval_ << "s" << std::endl;
    std::cout << "  - BatchSize: " << batchSize_ << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Consensus timer (for leaders to propose blocks)
    if (isLeaderNode_) {
        scheduleAt(simTime() + blockInterval_, consensusTimer_);
        std::cout << "[LEADER] Timer scheduled at t=" << (simTime() + blockInterval_) << "s" << std::endl;
    } else {
        std::cout << "[FOLLOWER] No timer" << std::endl;
    }
    
    // 🆕 交易生成定时器（所有节点都生成交易�?    if (autoGenerateTx_) {
        scheduleAt(simTime() + txGenerationInterval_, txGenerationTimer_);
        std::cout << "[TX-GEN] Timer scheduled at t=" << (simTime() + txGenerationInterval_) << "s" << std::endl;
    }
    
    // Shard maintenance timer (periodic rebalancing)
    scheduleAt(simTime() + 10.0, shardMaintenanceTimer_);
    
    // Reputation decay timer
    if (vrmEnabled_) {
        scheduleAt(simTime() + 5.0, reputationDecayTimer_);
    }
    
    // Heartbeat timer
    scheduleAt(simTime() + 1.0, heartbeatTimer_);
    
    // 🆕 所有节点都需要定期检查是否需要重新选举
    // 创建选举检查定时器（每5秒检查一次）
    cMessage* electionCheckTimer = new cMessage("ELECTION_CHECK");
    scheduleAt(simTime() + 5.0, electionCheckTimer);
}

// ============================================================================
// MESSAGE HANDLING
// ============================================================================

void TriBFTApp::onWSM(veins::BaseFrame1609_4* frame) {
    TriBFTMessage* tribftMsg = dynamic_cast<TriBFTMessage*>(frame);
    
    if (!tribftMsg) {
        DemoBaseApplLayer::onWSM(frame);
        return;
    }
    
    // 🔧 WORKAROUND: All disguised messages use TransactionMessage, handle them all first
    TransactionMessage* txMsg = dynamic_cast<TransactionMessage*>(tribftMsg);
    if (txMsg) {
        // First, let handleTransactionMessage process it (for forwarding, deduplication, etc.)
        handleTransactionMessage(txMsg);
        
        // Then check if it's a disguised consensus message that needs special handling
        std::string txID = txMsg->getTxID();
        if (txID.find("PROP_") == 0) {
            // This is a disguised PROPOSAL message - handle it after forwarding
            std::cout << "  [onWSM-DISGUISED] Processing PROPOSAL (txID=" << txID << ") from " 
                      << tribftMsg->getSenderID() << std::endl;
            handleDisguisedProposal(txMsg);
        } else if (txID.find("VOTE_") == 0) {
            // This is a disguised VOTE message - handle it after forwarding
            std::cout << "  [onWSM-DISGUISED] Processing VOTE (txID=" << txID << ") from " 
                      << tribftMsg->getSenderID() << std::endl;
            handleDisguisedVote(txMsg);
        } else if (txID.find("PHASE_") == 0) {
            // This is a disguised PhaseAdvance message - handle it after forwarding
            std::cout << "  [onWSM-DISGUISED] Processing PHASE-ADVANCE (txID=" << txID << ") from " 
                      << tribftMsg->getSenderID() << std::endl;
            handleDisguisedPhaseAdvance(txMsg);
        }
        return;
    }
    
    // Non-TransactionMessage handling
    switch (tribftMsg->getMessageType()) {
        case MT_TRANSACTION:
            // Already handled above
            break;
            break;
        case MT_PROPOSAL:
            handleProposalMessage(dynamic_cast<ProposalMessage*>(tribftMsg));
            break;
        case MT_VOTE_PREPARE:
        case MT_VOTE_PRE_COMMIT:
        case MT_VOTE_COMMIT:
            handleVoteMessage(dynamic_cast<VoteMessage*>(tribftMsg));
            break;
        case MT_DECIDE:
            handleDecideMessage(dynamic_cast<DecideMessage*>(tribftMsg));
            break;
        case MT_PHASE_ADVANCE:
            handlePhaseAdvanceMessage(dynamic_cast<PhaseAdvanceMessage*>(tribftMsg));
            break;
        case MT_SHARD_JOIN_REQUEST:
            handleShardJoinRequest(dynamic_cast<ShardJoinRequest*>(tribftMsg));
            break;
        case MT_SHARD_JOIN_RESPONSE:
            handleShardJoinResponse(dynamic_cast<ShardJoinResponse*>(tribftMsg));
            break;
        case MT_SHARD_UPDATE:
            handleShardUpdate(dynamic_cast<ShardUpdateMessage*>(tribftMsg));
            break;
        case MT_REPUTATION_UPDATE:
            handleReputationUpdate(dynamic_cast<ReputationUpdateMessage*>(tribftMsg));
            break;
        case MT_HEARTBEAT:
            handleHeartbeat(dynamic_cast<HeartbeatMessage*>(tribftMsg));
            break;
        default:
            EV_WARN << "[TriBFT] Unknown message type: " << tribftMsg->getMessageType() << endl;
            break;
    }
}

void TriBFTApp::onWSA(veins::DemoServiceAdvertisment* wsa) {
    // Not used in this application
}

void TriBFTApp::handleSelfMsg(cMessage* msg) {
    // 🔍 调试：输出收到的消息（高频，已禁用）
    // std::cout << "[SELF-MSG] Node " << nodeID_ << " received: " << msg->getName() 
    //           << " at t=" << simTime() << "s" << std::endl;
    
    if (msg == consensusTimer_) {
        handleConsensusTimer();
    }
    else if (msg == shardMaintenanceTimer_) {
        handleShardMaintenanceTimer();
    }
    else if (msg == reputationDecayTimer_) {
        handleReputationDecayTimer();
    }
    else if (msg == heartbeatTimer_) {
        handleHeartbeatTimer();
    }
    else if (msg == txGenerationTimer_) {
        // 处理交易生成定时器（高频日志已禁用）
        // std::cout << "[TX-GEN-TRIGGER] autoGenerateTx=" << autoGenerateTx_ << std::endl;
        if (autoGenerateTx_) {
            // 生成一笔交�?            Transaction tx = createTransaction();
            
            // 如果自己就是Leader，直接添加到交易�?            if (isLeaderNode_) {
                txPool_.push_back(tx);
                std::cout << "[TX-GENERATED] Leader " << nodeID_ << " added tx #" << tx.txID 
                          << " to pool (size: " << txPool_.size() << ")" << std::endl;
                
                // 检查是否达到批量大�?                if (txPool_.size() >= (size_t)batchSize_) {
                    std::cout << "[TX-POOL-FULL] Leader " << nodeID_ << " pool reached " << txPool_.size() 
                              << " txs (batchSize=" << batchSize_ << "), will propose in next consensus round" << std::endl;
                }
            } else {
                // 获取本分片的Leader ID
                NodeID leaderID = shardManager_->getShardLeader(currentShardID_);
                
                if (!leaderID.empty()) {
                    // 广播交易（启用多跳转发）
                    TransactionMessage* txMsg = new TransactionMessage();
                    txMsg->setSenderID(nodeID_.c_str());
                    txMsg->setTxID(tx.txID.c_str());
                    txMsg->setTxData(tx.data.c_str());
                    txMsg->setTimestamp(simTime());
                    txMsg->setHopCount(0);  // 🆕 初始跳数�?
                    
                    // 🆕🆕 智能转发：设置初始距离和目标分片
                    double myDistance = getDistanceToLeader();
                    txMsg->setSenderDistanceToLeader(myDistance);
                    txMsg->setTargetShardId(currentShardID_);  // 目标分片为当前分�?                    
                    // 设置为广播（多跳转发需要）
                    txMsg->setRecipientAddress(-1);
                    txMsg->setChannelNumber(static_cast<int>(veins::Channel::cch));
                    
                    sendDown(txMsg);
                    
                    std::cout << "[TX-GEN] Node " << nodeID_ << " generated tx #" << tx.txID 
                              << " (shard=" << currentShardID_ 
                              << ", distToLeader=" << std::fixed << std::setprecision(0) << myDistance << "m)" << std::endl;
                } else {
                    std::cout << "[TX-ERROR] Node " << nodeID_ << " cannot find Leader for shard " 
                              << currentShardID_ << std::endl;
                }
            }
            
            // 重新调度下一次交易生�?            scheduleAt(simTime() + txGenerationInterval_, txGenerationTimer_);
        }
    }
    else if (strcmp(msg->getName(), "ELECTION_CHECK") == 0) {
        // 所有节点定期检查是否需要选举
        if (needsReelection()) {
            std::cout << "[ELECTION_CHECK] Node " << nodeID_ << " triggering election at t=" << simTime() << std::endl;
            electConsensusGroup();
        }
        // 重新调度下一次检�?        scheduleAt(simTime() + 5.0, msg);
    }
    else {
        DemoBaseApplLayer::handleSelfMsg(msg);
    }
}

void TriBFTApp::handlePositionUpdate(cObject* obj) {
    DemoBaseApplLayer::handlePositionUpdate(obj);
    
    if (!isInitialized_) return;
    
    // Update location in shard manager
    GeoCoord newLocation = getCurrentLocation();
    ShardID newShardID = shardManager_->updateNodeLocation(nodeID_, newLocation);
    
    if (newShardID != currentShardID_ && newShardID != -1) {
        EV_INFO << "[TriBFT] Moved to new shard " << newShardID << endl;
        currentShardID_ = newShardID;
        
        // Re-initialize consensus with new shard
        initializeConsensus();
    }
}

// ============================================================================
// DISGUISED MESSAGE HANDLERS (WORKAROUND)
// ============================================================================

void TriBFTApp::handleDisguisedProposal(TransactionMessage* msg) {
    std::string txData = msg->getTxData();
    std::string senderID = msg->getSenderID();
    
    // Parse PROPOSAL data (format: "proposalID|blockHash|height|leaderID|txCount")
    std::istringstream iss(txData);
    std::string proposalID, blockHash, leaderID;
    int blockHeight, txCount;
    
    std::getline(iss, proposalID, '|');
    std::getline(iss, blockHash, '|');
    iss >> blockHeight;
    iss.ignore(1);  // skip '|'
    std::getline(iss, leaderID, '|');
    iss >> txCount;
    
    std::cout << "  [RECV] Got disguised PROPOSAL " << proposalID 
              << " from " << senderID << " (height=" << blockHeight << ", txs=" << txCount << ")" << std::endl;
    
    // Vote on the proposal
    std::cout << "  [VOTE] " << nodeID_ << " voting YES for " << proposalID << std::endl;
    
    VoteInfo vote;
    vote.voterID = nodeID_;
    vote.proposalID = proposalID;
    vote.phase = ConsensusPhase::PREPARE;
    vote.approve = true;
    vote.signature = "sig_" + nodeID_;
    
    sendVote(vote);
    
    // Leader processes its own vote
    if (nodeID_ == leaderID) {
        consensusEngine_->handleVote(vote);
    }
}

void TriBFTApp::handleDisguisedVote(TransactionMessage* msg) {
    std::string txData = msg->getTxData();
    std::string senderID = msg->getSenderID();
    
    // Parse VOTE data (format: "proposalID|phase|approve|signature")
    std::istringstream iss(txData);
    std::string proposalID, signature, approveStr;
    int phase;
    
    std::getline(iss, proposalID, '|');
    iss >> phase;
    iss.ignore(1);  // skip '|'
    std::getline(iss, approveStr, '|');
    std::getline(iss, signature);
    
    bool approve = (approveStr == "1");
    
    std::cout << "  [RECV-VOTE] From " << senderID << " for " << proposalID 
              << " phase=" << phase << " approve=" << approve << std::endl;
    
    VoteInfo vote;
    vote.voterID = senderID;
    vote.proposalID = proposalID;
    vote.phase = static_cast<ConsensusPhase>(phase);
    vote.approve = approve;
    vote.signature = signature;
    
    // Process the vote
    consensusEngine_->handleVote(vote);
}

void TriBFTApp::handleDisguisedPhaseAdvance(TransactionMessage* msg) {
    std::string txData = msg->getTxData();
    std::string senderID = msg->getSenderID();
    
    // Parse PhaseAdvance data (format: "proposalID|fromPhase|toPhase")
    std::istringstream iss(txData);
    std::string proposalID;
    int fromPhase, toPhase;
    
    std::getline(iss, proposalID, '|');
    iss >> fromPhase;
    iss.ignore(1);  // skip '|'
    iss >> toPhase;
    
    std::cout << "  [RECV-PHASE-ADV] From " << senderID << " for " << proposalID 
              << ": phase " << fromPhase << " -> " << toPhase << std::endl;
    
    // Pass to consensus engine
    consensusEngine_->handlePhaseAdvance(
        proposalID,
        static_cast<ConsensusPhase>(toPhase)
    );
}

// ============================================================================
// SPECIFIC MESSAGE HANDLERS
// ============================================================================

void TriBFTApp::handleTransactionMessage(TransactionMessage* msg) {
    if (!msg) return;
    
    std::string txID = msg->getTxID();
    int hopCount = msg->getHopCount();
    double senderDistance = msg->getSenderDistanceToLeader();
    int targetShardId = msg->getTargetShardId();
    
    // 🔍 Debug for disguised messages
    bool isDisguised = (txID.find("PROP_") == 0 || txID.find("VOTE_") == 0 || txID.find("PHASE_") == 0);
    if (isDisguised) {
        std::cout << "  [TX-HANDLER-DEBUG] Processing disguised msg: txID=" << txID 
                  << ", hop=" << hopCount << ", targetShard=" << targetShardId 
                  << ", myShard=" << currentShardID_ << std::endl;
    }
    
    // 🆕 防止循环转发：检查是否已经见过这笔交�?    if (seenTxIds_.find(txID) != seenTxIds_.end()) {
        // 已经处理过，直接丢弃
        if (isDisguised) std::cout << "  [TX-HANDLER-DEBUG] Already seen, discarding" << std::endl;
        return;
    }
    
    // 标记为已�?    seenTxIds_.insert(txID);
    
    // 🆕 智能转发：分片过�?    // 只处理本分片的交易（targetShardId == -1 表示广播，或者等于当前分片）
    if (targetShardId != -1 && !isInTargetShard(targetShardId)) {
        // 不属于本分片，丢�?        if (isDisguised) std::cout << "  [TX-HANDLER-DEBUG] Wrong shard, discarding" << std::endl;
        return;
    }
    
    // 如果是Leader，接收到交易�?    if (isLeaderNode_) {
        Transaction tx;
        tx.txID = txID;
        tx.data = msg->getTxData();
        tx.timestamp = msg->getTimestamp().dbl();
        tx.sender = msg->getSenderID();
        
        txPool_.push_back(tx);
        
        std::cout << "[TX-RECEIVED] Leader " << nodeID_ << " received tx #" << tx.txID 
                  << " from " << tx.sender << " (hops=" << hopCount 
                  << ", senderDist=" << std::fixed << std::setprecision(0) << senderDistance << "m"
                  << ", pool size: " << txPool_.size() << ")" << std::endl;
        
        // 检查交易池是否达到批量大小
        if (txPool_.size() >= (size_t)batchSize_) {
            std::cout << "[TX-POOL-FULL] Leader " << nodeID_ << " pool reached " << txPool_.size() 
                      << " txs (batchSize=" << batchSize_ << "), will propose in next consensus round" << std::endl;
        }
        return;
    }
    
    // 🆕🆕🆕 智能方向转发：只转发给更接近Leader的节�?    if (enableMultiHop_ && hopCount < maxHops_) {
        // 获取本分片的Leader
        NodeID leaderID = shardManager_->getShardLeader(currentShardID_);
        
        if (leaderID.empty()) {
            // 没有Leader，无法转�?            return;
        }
        
        // 🎯 关键：智能判断是否应该转�?        // 只有当我比发送者更接近Leader时才转发
        if (!shouldForwardTransaction(senderDistance)) {
            // 我离Leader比发送者远，不转发（避免无效转发）
            return;
        }
        
        // 创建转发消息
        TransactionMessage* fwdMsg = msg->dup();
        fwdMsg->setHopCount(hopCount + 1);
        
        // 🆕 更新发送者距离为当前节点到Leader的距�?        double myDistance = getDistanceToLeader();
        fwdMsg->setSenderDistanceToLeader(myDistance);
        
        fwdMsg->setRecipientAddress(-1);  // 广播
        fwdMsg->setChannelNumber(static_cast<int>(veins::Channel::cch));
        
        sendDown(fwdMsg);
        
        std::cout << "[TX-FORWARD-SMART] Node " << nodeID_ << " forwarded tx #" << txID 
                  << " (hop " << (hopCount + 1) << "/" << maxHops_ 
                  << ", prevDist=" << std::fixed << std::setprecision(0) << senderDistance << "m"
                  << ", myDist=" << myDistance << "m"
                  << ", saved=" << (senderDistance - myDistance) << "m)" << std::endl;
    }
}

void TriBFTApp::handleProposalMessage(ProposalMessage* msg) {
    if (!msg) return;
    
    std::cout << "  [RECV] " << nodeID_ << " got proposal " << msg->getProposalID() 
              << " from " << msg->getLeaderID() 
              << " height=" << msg->getBlockHeight() << std::endl;
    
    // 🆕 首先同步区块高度（在检查角色之前）
    // 即使是ORDINARY节点也需要同步高度以保持一致�?    BlockHeight proposalHeight = msg->getBlockHeight();
    BlockHeight currentHeight = consensusEngine_->getCurrentHeight();
    if (proposalHeight > currentHeight + 1) {
        std::cout << "  [SYNC] " << nodeID_ << " syncing height from " 
                  << currentHeight << " to " << (proposalHeight - 1) << std::endl;
        // 在实际系统中，这里应该请求缺失的区块
        // 简化处理：直接更新高度（假设已经同步了缺失的区块）
        consensusEngine_->syncToHeight(proposalHeight - 1);
    }
    
    // 🆕 自动更新节点角色（follower节点查询共识组）
    if (nodeRole_ == NodeRole::ORDINARY && shardManager_) {
        NodeRole newRole = shardManager_->getNodeRole(nodeID_, currentShardID_);
        if (newRole != NodeRole::ORDINARY) {
            nodeRole_ = newRole;
            std::cout << "  [ROLE-UPDATE] " << nodeID_ << " updated role to " << (int)newRole << std::endl;
        }
    }
    
    // 🆕 检查是否在共识群组�?    if (!shouldParticipateInConsensus()) {
        // 普通节点收到提案，只同步不投票
        std::cout << "  [ORDINARY] Received proposal but not participating (role=" << (int)nodeRole_ << ")" << std::endl;
        // 冗余节点可以同步区块，但不投�?        if (nodeRole_ == NodeRole::CONSENSUS_REDUNDANT) {
            std::cout << "  [REDUNDANT] Syncing block data" << std::endl;
        }
        return;
    }
    
    // Convert to internal format
    ConsensusProposal proposal;
    proposal.proposalID = msg->getProposalID();
    proposal.blockHash = msg->getBlockHash();
    proposal.blockHeight = msg->getBlockHeight();
    proposal.leaderID = msg->getLeaderID();
    proposal.shardID = msg->getShardID();
    proposal.viewNumber = msg->getViewNumber();
    proposal.proposalTime = msg->getTimestamp();
    
    // 🔧 FIX: Don't parse transactions from PROPOSAL (txData removed to reduce message size)
    // Consensus members only vote on the block hash, they don't need full transaction data
    // Leader already has the transactions in its pool
    proposal.transactions.clear();  // Empty for now (not needed for voting)
    
    // Pass to consensus engine
    consensusEngine_->handleProposal(proposal);
}

void TriBFTApp::handleVoteMessage(VoteMessage* msg) {
    if (!msg) return;
    
    std::cout << "  [VOTE-RECV] " << nodeID_ << " got vote from " << msg->getSenderID() 
              << " (" << (msg->getApprove() ? "YES" : "NO") << ")" << std::endl;
    
    // Convert to internal format
    tribft::VoteInfo vote;
    vote.proposalID = msg->getProposalID();
    vote.voterID = msg->getSenderID();
    vote.phase = static_cast<ConsensusPhase>(msg->getPhase());
    vote.approve = msg->getApprove();
    vote.signature = msg->getSignature();
    vote.voteTime = msg->getTimestamp();
    
    // Pass to consensus engine
    consensusEngine_->handleVote(vote);
}

void TriBFTApp::handlePhaseAdvanceMessage(PhaseAdvanceMessage* msg) {
    if (!msg) return;
    
    std::cout << "  [PHASE-ADV-RECV] " << nodeID_ << " got phase advance from " << msg->getSenderID()
              << ": " << msg->getFromPhase() << " -> " << msg->getToPhase() << std::endl;
    
    // Pass to consensus engine
    consensusEngine_->handlePhaseAdvance(
        msg->getProposalID(),
        static_cast<ConsensusPhase>(msg->getToPhase())
    );
}

void TriBFTApp::handleDecideMessage(DecideMessage* msg) {
    if (!msg) return;
    
    EV_INFO << "[TriBFT] Received decision for block " << msg->getBlockHeight() 
            << " (" << (msg->getCommitted() ? "COMMITTED" : "REJECTED") << ")" << endl;
}

void TriBFTApp::handleShardJoinRequest(ShardJoinRequest* msg) {
    // Leaders handle join requests
    if (!isLeaderNode_) return;
    
    EV_INFO << "[TriBFT] Processing shard join request from " << msg->getSenderID() << endl;
    
    // Add node to shard
    GeoCoord location(msg->getLatitude(), msg->getLongitude());
    ShardID assignedShard = shardManager_->addNode(
        msg->getSenderID(), 
        location, 
        msg->getReputationScore()
    );
    
    // Send response
    ShardJoinResponse* response = new ShardJoinResponse();
    response->setSenderID(nodeID_.c_str());
    response->setAssignedShardID(assignedShard);
    response->setAccepted(assignedShard != -1);
    response->setLeaderID(nodeID_.c_str());
    
    const ShardInfo* shard = shardManager_->getShardInfo(assignedShard);
    if (shard) {
        response->setMemberCount(shard->getMemberCount());
    }
    
    sendDown(response);
}

void TriBFTApp::handleShardJoinResponse(ShardJoinResponse* msg) {
    EV_INFO << "[TriBFT] Joined shard " << msg->getAssignedShardID() 
            << " with " << msg->getMemberCount() << " members" << endl;
}

void TriBFTApp::handleShardUpdate(ShardUpdateMessage* msg) {
    EV_INFO << "[TriBFT] Shard update: leader=" << msg->getLeaderID() 
            << ", members=" << msg->getMemberCount() << endl;
}

void TriBFTApp::handleReputationUpdate(ReputationUpdateMessage* msg) {
    if (!vrmEnabled_) return;
    
    EV_DEBUG << "[VRM] Reputation update for " << msg->getTargetNodeID() 
             << ": " << msg->getNewScore() << endl;
}

void TriBFTApp::handleHeartbeat(HeartbeatMessage* msg) {
    EV_DEBUG << "[TriBFT] Heartbeat from " << msg->getSenderID() << endl;
}

// ============================================================================
// CONSENSUS CALLBACKS
// ============================================================================

void TriBFTApp::onProposalGenerated(const ConsensusProposal& proposal) {
    EV_INFO << "[TriBFT] Broadcasting proposal " << proposal.proposalID 
            << " with " << proposal.transactions.size() << " transactions" << endl;
    sendProposal(proposal);
}

void TriBFTApp::onVoteGenerated(const tribft::VoteInfo& vote) {
    sendVote(vote);
}

void TriBFTApp::onBlockCommitted(const Block& block) {
    EV_INFO << "[TriBFT] Block " << block.height << " committed with " 
            << block.transactions.size() << " transactions" << endl;
    
    // 🆕 更新已提交区块计�?    committedBlockCount_++;
    
    // Emit statistics
    emit(blockCommittedSignal_, 1L);
    
    // Update reputation for participants
    if (vrmEnabled_) {
        std::vector<NodeID> participants;
        for (const auto& vote : block.qc.votes) {
            participants.push_back(vote.voterID);
        }
        reputationManager_->updateForConsensusSuccess(participants);
    }
    
    // Send decision to others
    sendDecision(block);
}

void TriBFTApp::onConsensusLog(const std::string& message) {
    EV_DEBUG << "[Consensus] " << message << endl;
}

// ============================================================================
// TIMER HANDLERS
// ============================================================================

void TriBFTApp::handleConsensusTimer() {
    std::cout << "[CONSENSUS TIMER] Node " << nodeID_ << " triggered at t=" << simTime() << "s" << std::endl;
    
    // 🆕 检查是否需要重新选举
    if (needsReelection()) {
        std::cout << "  [REELECTION] Triggering new election for epoch " << getCurrentEpoch() << std::endl;
        electConsensusGroup();
    }
    
    // 🆕 检查是否是共识群组成员
    if (!shouldParticipateInConsensus()) {
        std::cout << "  [ORDINARY] Not in consensus group, skipping" << std::endl;
        // 🔧 修复：重新调度前先取�?        if (consensusTimer_->isScheduled()) {
            cancelEvent(consensusTimer_);
        }
        scheduleAt(simTime() + blockInterval_, consensusTimer_);
        return;
    }
    
    if (!isLeaderNode_) {
        std::cout << "[ERROR] Non-leader received timer!" << std::endl;
        return;
    }
    
    // Generate transactions
    size_t txBefore = txPool_.size();
    generateTransactions();
    size_t txAfter = txPool_.size();
    
    std::cout << "  TX: " << txBefore << "->" << txAfter 
              << " (need " << batchSize_ << ")" << std::endl;
    
    // Propose block if we have transactions
    if (txPool_.size() >= static_cast<size_t>(batchSize_)) {
        std::vector<tribft::Transaction> batch(txPool_.begin(), txPool_.begin() + batchSize_);
        txPool_.erase(txPool_.begin(), txPool_.begin() + batchSize_);
        
        std::cout << "  [PROPOSE] Block with " << batch.size() << " tx" << std::endl;
        
        if (consensusEngine_->proposeBlock(batch)) {
            std::cout << "  [SUCCESS] Proposal OK!" << std::endl;
        } else {
            std::cout << "  [FAILED] Proposal FAILED!" << std::endl;
        }
    } else {
        std::cout << "  [WAIT] Need more TX (" << txPool_.size() 
                << "/" << batchSize_ << ")" << std::endl;
    }
    
    // Schedule next consensus round
    // 🔧 修复：重新调度前先取�?    if (consensusTimer_->isScheduled()) {
        cancelEvent(consensusTimer_);
    }
    scheduleAt(simTime() + blockInterval_, consensusTimer_);
    EV_INFO << "  �?Next timer at t=" << (simTime() + blockInterval_) << "s" << endl;
}

void TriBFTApp::handleShardMaintenanceTimer() {
    // Rebalance shards
    shardManager_->rebalanceShards();
    
    // Update leader status
    bool wasLeader = isLeaderNode_;
    isLeaderNode_ = shardManager_->isShardLeader(nodeID_, currentShardID_);
    
    if (wasLeader != isLeaderNode_) {
        EV_INFO << "[TriBFT] Leader status changed: " << (isLeaderNode_ ? "NOW LEADER" : "NOT LEADER") << endl;
        
        if (isLeaderNode_) {
            // 🔧 修复：重新调度前先取�?            if (consensusTimer_->isScheduled()) {
                cancelEvent(consensusTimer_);
            }
            scheduleAt(simTime() + blockInterval_, consensusTimer_);
        } else {
            if (consensusTimer_->isScheduled()) {
                cancelEvent(consensusTimer_);
            }
        }
    }
    
    // Emit shard statistics
    const ShardInfo* shard = shardManager_->getShardInfo(currentShardID_);
    if (shard) {
        emit(shardSizeSignal_, static_cast<long>(shard->getMemberCount()));
    }
    
    scheduleAt(simTime() + 10.0, shardMaintenanceTimer_);
}

void TriBFTApp::handleReputationDecayTimer() {
    if (vrmEnabled_) {
        reputationManager_->applyDecay();
        
        // Emit reputation signal
        double rep = reputationManager_->getReputation(nodeID_);
        emit(reputationSignal_, rep);
    }
    
    scheduleAt(simTime() + 5.0, reputationDecayTimer_);
}

void TriBFTApp::handleHeartbeatTimer() {
    sendHeartbeat();
    scheduleAt(simTime() + 1.0, heartbeatTimer_);
}

// ============================================================================
// TRANSACTION GENERATION
// ============================================================================

void TriBFTApp::generateTransactions() {
    // Generate random transactions
    int numTx = intuniform(1, 5);
    
    // 🔍 详细日志：交易生�?    EV_DEBUG << "    💰 Generating " << numTx << " transactions..." << endl;
    
    for (int i = 0; i < numTx; i++) {
        txPool_.push_back(createTransaction());
    }
    
    EV_DEBUG << "    💼 Transaction pool size: " << txPool_.size() << endl;
}

tribft::Transaction TriBFTApp::createTransaction() {
    tribft::Transaction tx;
    tx.txID = nodeID_ + "_tx_" + std::to_string(txCounter_++);
    tx.sender = nodeID_;
    tx.receiver = "node_" + std::to_string(intuniform(0, 99));
    tx.value = uniform(1.0, 100.0);
    tx.timestamp = simTime();
    tx.data = "Sample transaction data";
    return tx;
}

// ============================================================================
// SENDING HELPERS
// ============================================================================

void TriBFTApp::sendProposal(const ConsensusProposal& proposal) {
    // 🔧 WORKAROUND: Disguise PROPOSAL as TransactionMessage (only TX can be transmitted)
    TransactionMessage* msg = new TransactionMessage();
    
    msg->setSenderID(nodeID_.c_str());
    msg->setShardID(proposal.shardID);
    msg->setViewNumber(proposal.viewNumber);
    msg->setTimestamp(simTime());
    
    // 🔧 Mark this as a disguised PROPOSAL message
    msg->setActualMessageType(MT_PROPOSAL);
    
    // Serialize PROPOSAL data into txData field (format: "proposalID|blockHash|height|leaderID|txCount")
    std::ostringstream oss;
    oss << proposal.proposalID << "|"
        << proposal.blockHash << "|"
        << proposal.blockHeight << "|"
        << proposal.leaderID << "|"
        << proposal.transactions.size();
    msg->setTxData(oss.str().c_str());
    // 🔧 WORKAROUND: Use txID prefix to identify message type (Veins doesn't transmit actualMessageType)
    std::string txID = "PROP_" + proposal.proposalID;
    msg->setTxID(txID.c_str());
    
    // Set Veins network parameters (same as real TX)
    msg->setRecipientAddress(-1);  // broadcast to all
    msg->setChannelNumber(static_cast<int>(veins::Channel::cch));
    msg->setHopCount(0);
    msg->setSenderDistanceToLeader(-1.0);
    msg->setTargetShardId(proposal.shardID);
    
    std::cout << "  [SEND-PROPOSAL-DISGUISED] " << proposal.proposalID << " as TX" << std::endl;
    std::cout << "  [DEBUG-SEND] actualType=" << msg->getActualMessageType() 
              << " (MT_PROPOSAL=" << MT_PROPOSAL << ")" << std::endl;
    std::cout << "  [DEBUG-SEND] txID=" << msg->getTxID() 
              << ", txData=" << msg->getTxData() << std::endl;
    
    // 🔧 修复：立即本地处理自己的PROPOSAL（因为广播不会发送给自己�?    handleDisguisedProposal(msg);
    
    sendDown(msg);
    std::cout << "  [DEBUG-SEND] sendDown() completed" << std::endl;
}

void TriBFTApp::sendVote(const tribft::VoteInfo& vote) {
    // 🔧 WORKAROUND: Disguise VOTE as TransactionMessage
    TransactionMessage* msg = new TransactionMessage();
    
    msg->setSenderID(vote.voterID.c_str());
    msg->setTimestamp(simTime());
    
    // Determine vote type
    int voteType = static_cast<int>(vote.phase) == 1 ? MT_VOTE_PREPARE : 
                   static_cast<int>(vote.phase) == 2 ? MT_VOTE_PRE_COMMIT : MT_VOTE_COMMIT;
    msg->setActualMessageType(voteType);
    
    // Serialize VOTE data (format: "proposalID|phase|approve|signature")
    std::ostringstream oss;
    oss << vote.proposalID << "|"
        << static_cast<int>(vote.phase) << "|"
        << (vote.approve ? "1" : "0") << "|"
        << vote.signature;
    msg->setTxData(oss.str().c_str());
    // 🔧 WORKAROUND: Use txID prefix to identify message type
    std::string txID = "VOTE_" + vote.proposalID + "_" + vote.voterID;
    msg->setTxID(txID.c_str());
    
    // Set Veins network parameters
    msg->setRecipientAddress(-1);
    msg->setChannelNumber(static_cast<int>(veins::Channel::cch));
    msg->setHopCount(0);
    msg->setSenderDistanceToLeader(-1.0);
    msg->setTargetShardId(currentShardID_);
    
    std::cout << "  [VOTE-DISGUISED] " << nodeID_ << " voting " 
              << (vote.approve ? "YES" : "NO") << " for " << vote.proposalID << " (as TX)" << std::endl;
    
    // 🔧 修复：立即本地处理自己的投票（因为广播不会发送给自己�?    consensusEngine_->handleVote(vote);
    
    // 然后广播给其他节�?    sendDown(msg);
}

void TriBFTApp::sendPhaseAdvance(const std::string& proposalID, ConsensusPhase fromPhase, ConsensusPhase toPhase) {
    // 🔧 WORKAROUND: Disguise PhaseAdvance as TransactionMessage
    TransactionMessage* msg = new TransactionMessage();
    
    msg->setSenderID(nodeID_.c_str());
    msg->setTimestamp(simTime());
    
    // Mark as disguised PhaseAdvance
    msg->setActualMessageType(MT_PHASE_ADVANCE);
    
    // Serialize PhaseAdvance data (format: "proposalID|fromPhase|toPhase")
    std::ostringstream oss;
    oss << proposalID << "|" << static_cast<int>(fromPhase) << "|" << static_cast<int>(toPhase);
    msg->setTxData(oss.str().c_str());
    
    // 🔧 WORKAROUND: Use txID prefix to identify message type
    std::string txID = "PHASE_" + proposalID + "_" + std::to_string(static_cast<int>(toPhase));
    msg->setTxID(txID.c_str());
    
    // Set Veins network parameters
    msg->setRecipientAddress(-1);  // Broadcast
    msg->setChannelNumber(static_cast<int>(veins::Channel::cch));
    msg->setHopCount(0);
    msg->setSenderDistanceToLeader(-1.0);
    msg->setTargetShardId(currentShardID_);
    
    std::cout << "  [PHASE-ADV-SEND] " << nodeID_ << " broadcasting phase advance: " 
              << (int)fromPhase << " -> " << (int)toPhase << " for " << proposalID << " (as TX)" << std::endl;
    
    // 🔧 修复：立即本地处�?    handleDisguisedPhaseAdvance(msg);
    
    // Broadcast to all nodes
    sendDown(msg);
}

void TriBFTApp::sendDecision(const Block& block) {
    DecideMessage* msg = new DecideMessage();
    msg->setMessageType(MT_DECIDE);
    msg->setSenderID(nodeID_.c_str());
    msg->setProposalID(block.blockHash.c_str());
    msg->setBlockHash(block.blockHash.c_str());
    msg->setBlockHeight(block.height);
    msg->setCommitted(true);
    msg->setTimestamp(simTime());
    
    // 🔧 Set Veins network parameters
    msg->setRecipientAddress(-1);
    msg->setChannelNumber(static_cast<int>(veins::Channel::cch));
    msg->setPsid(-1);
    
    sendDown(msg);
}

void TriBFTApp::sendShardJoinRequest() {
    ShardJoinRequest* msg = new ShardJoinRequest();
    msg->setSenderID(nodeID_.c_str());
    
    GeoCoord loc = getCurrentLocation();
    msg->setLatitude(loc.latitude);
    msg->setLongitude(loc.longitude);
    msg->setReputationScore(initialReputation_);
    msg->setTimestamp(simTime());
    
    sendDown(msg);
}

void TriBFTApp::sendShardUpdate() {
    const ShardInfo* shard = shardManager_->getShardInfo(currentShardID_);
    if (!shard) return;
    
    ShardUpdateMessage* msg = new ShardUpdateMessage();
    msg->setSenderID(nodeID_.c_str());
    msg->setShardID(currentShardID_);
    msg->setLeaderID(shard->leader.c_str());
    msg->setMemberCount(shard->getMemberCount());
    msg->setCenterLat(shard->centerPoint.latitude);
    msg->setCenterLon(shard->centerPoint.longitude);
    msg->setRadius(shard->radius);
    msg->setTimestamp(simTime());
    
    sendDown(msg);
}

void TriBFTApp::sendHeartbeat() {
    HeartbeatMessage* msg = new HeartbeatMessage();
    msg->setSenderID(nodeID_.c_str());
    msg->setShardID(currentShardID_);
    msg->setCurrentLoad(0.0);
    msg->setActiveTxCount(txPool_.size());
    msg->setTimestamp(simTime());
    
    sendDown(msg);
}

// ============================================================================
// UTILITY
// ============================================================================

std::string TriBFTApp::getNodeID() const {
    return getParentModule()->getFullName();
}

GeoCoord TriBFTApp::getCurrentLocation() const {
    // For static nodes with BaseMobility, read directly from mobility submodule parameters
    cModule* parent = getParentModule();
    if (parent) {
        cModule* mobModule = parent->getSubmodule("mobility");
        if (mobModule) {
            // Try to get position from BaseMobility parameters
            if (mobModule->hasPar("x") && mobModule->hasPar("y")) {
                double x = mobModule->par("x").doubleValue();
                double y = mobModule->par("y").doubleValue();
                // 高频日志已禁�?                // std::cout << "[GET-LOCATION] " << nodeID_ << " from mobility params: (" << x << "," << y << ")" << std::endl;
                return GeoCoord(x, y);
            }
            
            // Fallback: try to cast to BaseMobility and get position
            veins::BaseMobility* baseMob = dynamic_cast<veins::BaseMobility*>(mobModule);
            if (baseMob) {
                veins::Coord pos = baseMob->getPositionAt(simTime());
                // 高频日志已禁�?                // std::cout << "[GET-LOCATION] " << nodeID_ << " from BaseMobility: (" << pos.x << "," << pos.y << ")" << std::endl;
                return GeoCoord(pos.x, pos.y);
            }
        }
    }
    
    // Fallback: use parent class mobility (TraCIMobility) if available
    if (mobility) {
        // 高频日志已禁�?        // std::cout << "[GET-LOCATION] " << nodeID_ << " using TraCIMobility" << std::endl;
        veins::Coord pos = mobility->getPositionAt(simTime());
        return GeoCoord(pos.x, pos.y);
    }
    
    std::cerr << "[ERROR] Cannot get location for " << nodeID_ << std::endl;
    return GeoCoord(0, 0);
}

bool TriBFTApp::isLeader() const {
    return isLeaderNode_;
}

// ============================================================================
// SMART FORWARDING HELPERS (智能转发辅助函数)
// ============================================================================

double TriBFTApp::getDistanceToLeader() const {
    if (!shardManager_) return -1.0;
    
    // 获取本分片的Leader
    NodeID leaderID = shardManager_->getShardLeader(currentShardID_);
    if (leaderID.empty()) {
        return -1.0;  // 没有Leader
    }
    
    if (leaderID == nodeID_) {
        return 0.0;  // 自己就是Leader
    }
    
    // 🆕 RSU优先策略：使用Leader的真实位置（RSU位置固定，准确可靠）
    GeoCoord leaderPos = shardManager_->getNodeLocation(leaderID);
    if (leaderPos.latitude == 0.0 && leaderPos.longitude == 0.0) {
        // Leader位置未知，降级使用分片中心点
        const ShardInfo* shardInfo = shardManager_->getShardInfo(currentShardID_);
        if (!shardInfo) {
            return -1.0;
        }
        leaderPos = shardInfo->centerPoint;
    }
    
    GeoCoord myPos = getCurrentLocation();
    
    // 使用GeoCoord的distanceTo方法计算到Leader的真实距�?    double distance = myPos.distanceTo(leaderPos);
    
    return distance;
}

bool TriBFTApp::shouldForwardTransaction(double senderDistance) const {
    // 🔧 快速修复：暂时禁用距离判断
    // 原因：Leader是移动节点，位置不断变化，导致距离判断失�?    // 解决方案：只依赖分片过滤（isInTargetShard在调用处已检查）
    // 
    // 优点�?    //   - 覆盖率：1.21% �?20-30%
    //   - 能够生成区块
    //   - 保留分片隔离
    // 
    // 长期方案：创建真正的固定RSU节点作为Leader
    return true;
}

bool TriBFTApp::isInTargetShard(int targetShardId) const {
    // targetShardId == -1 表示广播给所有分�?    if (targetShardId == -1) {
        return true;
    }
    
    // 检查是否为本分�?    return (targetShardId == currentShardID_);
}

void TriBFTApp::logInfo(const std::string& message) {
    EV_INFO << "[TriBFT] " << message << endl;
}

void TriBFTApp::recordStatistics() {
    // Final statistics
    if (consensusEngine_) {
        const ConsensusMetrics& metrics = consensusEngine_->getMetrics();
        EV_INFO << "[Stats] Total proposals: " << metrics.totalProposals << endl;
        EV_INFO << "[Stats] Successful commits: " << metrics.successfulCommits << endl;
        EV_INFO << "[Stats] Failed consensus: " << metrics.failedConsensus << endl;
        EV_INFO << "[Stats] Average latency: " << metrics.avgLatency << "s" << endl;
        EV_INFO << "[Stats] Throughput: " << metrics.throughput << " TPS" << endl;
    }
    
    if (reputationManager_ && vrmEnabled_) {
        auto stats = reputationManager_->getStatistics();
        EV_INFO << "[Stats] Total nodes: " << stats.totalNodes << endl;
        EV_INFO << "[Stats] Reliable nodes: " << stats.reliableNodes << endl;
        EV_INFO << "[Stats] Average reputation: " << stats.averageScore << endl;
    }
}

// ============================================================================
// 🆕 共识群组管理 (P1)
// ============================================================================

void TriBFTApp::electConsensusGroup() {
    if (!shardManager_) {
        return;
    }
    
    int currentEpoch = getCurrentEpoch();
    
    // 触发VRF选举
    ConsensusGroup group = shardManager_->electConsensusGroup(currentShardID_, currentEpoch);
    
    // 更新本节点的角色
    nodeRole_ = shardManager_->getNodeRole(nodeID_, currentShardID_);
    lastElectionEpoch_ = currentEpoch;
    
    // 打印选举结果
    std::string roleStr;
    switch (nodeRole_) {
        case NodeRole::ORDINARY: roleStr = "ORDINARY"; break;
        case NodeRole::CONSENSUS_PRIMARY: roleStr = "PRIMARY"; break;
        case NodeRole::CONSENSUS_REDUNDANT: roleStr = "REDUNDANT"; break;
        case NodeRole::RSU_PERMANENT: roleStr = "RSU"; break;
    }
    
    // 使用std::cout输出，确保在命令行可�?    std::cout << ">>>GROUP_ELECTION<<< Node:" << nodeID_ 
              << " Role:" << roleStr 
              << " Epoch:" << currentEpoch 
              << " GroupSize:" << group.getTotalSize() 
              << " Primary:" << group.primaryNodes.size()
              << " Redundant:" << group.redundantNodes.size()
              << std::endl;
    
    // 更新共识引擎的分片大小（只有共识群组大小，而非整个分片�?    if (consensusEngine_) {
        consensusEngine_->setShardSize(group.getTotalSize());
    }
}

bool TriBFTApp::needsReelection() const {
    int currentEpoch = getCurrentEpoch();
    
    // 初次选举：如果从未选举过（lastElectionEpoch_ == -1），需要选举
    if (lastElectionEpoch_ == -1) {
        return true;
    }
    
    // 定期重选：每个epoch重新选举（基于已提交的区块数�?    if (currentEpoch > lastElectionEpoch_) {
        return true;
    }
    
    return false;
}

int TriBFTApp::getCurrentEpoch() const {
    return committedBlockCount_ / epochBlocks_;
}

bool TriBFTApp::shouldParticipateInConsensus() const {
    // 共识主节点、冗余节点和RSU都参与共识投�?    // redundant节点作为热备份，能够立即接管
    return (nodeRole_ == NodeRole::CONSENSUS_PRIMARY || 
            nodeRole_ == NodeRole::CONSENSUS_REDUNDANT ||
            nodeRole_ == NodeRole::RSU_PERMANENT);
}

} // namespace tribft

