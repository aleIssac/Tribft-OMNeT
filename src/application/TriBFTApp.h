#ifndef TRIBFT_APP_H
#define TRIBFT_APP_H

#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "../messages/TriBFTMessage_m.h"
#include "../common/TriBFTDefs.h"
#include "../shard/RegionalShardManager.h"
#include "../consensus/HotStuffEngine.h"
#include "../reputation/VRMManager.h"
#include <set>  // 🆕 用于seenTxIds_

namespace tribft {

/**
 * @brief TriBFT Application Layer
 * 
 * Main application that integrates:
 * - Regional Shard Management
 * - HotStuff Consensus Engine
 * - VRM Reputation System
 * - Veins V2X Communication
 * 
 * Design Principles:
 * - SOLID: Delegates responsibilities to specialized components
 * - KISS: Clear message handling and state management
 * - YAGNI: Only implement essential integration logic
 */
class TriBFTApp : public veins::DemoBaseApplLayer {
public:
    TriBFTApp() = default;
    ~TriBFTApp() override = default;
    
    // OMNeT++ lifecycle
    void initialize(int stage) override;
    void finish() override;
    
protected:
    // Message handling
    void onWSM(veins::BaseFrame1609_4* frame) override;
    void onWSA(veins::DemoServiceAdvertisment* wsa) override;
    
    // Beacon handling
    void handleSelfMsg(cMessage* msg) override;
    void handlePositionUpdate(cObject* obj) override;
    
private:
    // ========================================================================
    // INITIALIZATION HELPERS
    // ========================================================================
    
    void initializeShard();
    void initializeConsensus();
    void initializeReputation();
    void initializeTimers();
    
    // ========================================================================
    // MESSAGE HANDLERS (by type)
    // ========================================================================
    
    void handleProposalMessage(ProposalMessage* msg);
    void handleVoteMessage(VoteMessage* msg);
    void handleDecideMessage(DecideMessage* msg);
    void handlePhaseAdvanceMessage(PhaseAdvanceMessage* msg);
    void handleShardJoinRequest(ShardJoinRequest* msg);
    void handleShardJoinResponse(ShardJoinResponse* msg);
    void handleShardUpdate(ShardUpdateMessage* msg);
    void handleReputationUpdate(ReputationUpdateMessage* msg);
    void handleHeartbeat(HeartbeatMessage* msg);
    void handleTransactionMessage(TransactionMessage* msg);  // 🆕 交易消息处理（含多跳转发）
    
    // 🔧 WORKAROUND: Disguised message handlers
    void handleDisguisedProposal(TransactionMessage* msg);
    void handleDisguisedVote(TransactionMessage* msg);
    void handleDisguisedPhaseAdvance(TransactionMessage* msg);
    
    // ========================================================================
    // CONSENSUS CALLBACKS
    // ========================================================================
    
    void onProposalGenerated(const ConsensusProposal& proposal);
    void onVoteGenerated(const tribft::VoteInfo& vote);
    void onBlockCommitted(const Block& block);
    void onConsensusLog(const std::string& message);
    
    // ========================================================================
    // TIMER HANDLERS
    // ========================================================================
    
    void handleConsensusTimer();
    void handleShardMaintenanceTimer();
    void handleReputationDecayTimer();
    void handleHeartbeatTimer();
    
    // ========================================================================
    // TRANSACTION GENERATION
    // ========================================================================
    
    void generateTransactions();
    tribft::Transaction createTransaction();
    
    // ========================================================================
    // SENDING HELPERS
    // ========================================================================
    
    void sendProposal(const ConsensusProposal& proposal);
    void sendVote(const tribft::VoteInfo& vote);
    void sendDecision(const Block& block);
    void sendPhaseAdvance(const std::string& proposalID, ConsensusPhase fromPhase, ConsensusPhase toPhase);
    void sendShardJoinRequest();
    void sendShardUpdate();
    void sendHeartbeat();
    
    // ========================================================================
    // UTILITY
    // ========================================================================
    
    std::string getNodeID() const;
    GeoCoord getCurrentLocation() const;
    bool isLeader() const;
    
    void logInfo(const std::string& message);
    void recordStatistics();
    
    // ========================================================================
    // SMART FORWARDING HELPERS (智能转发辅助函数)
    // ========================================================================
    
    /**
     * @brief 计算当前节点到Leader的距离
     * @return 距离（米），如果无Leader返回-1
     */
    double getDistanceToLeader() const;
    
    /**
     * @brief 判断是否应该转发交易（智能方向判断）
     * @param senderDistance 发送者到Leader的距离
     * @return true 如果我比发送者更接近Leader
     */
    bool shouldForwardTransaction(double senderDistance) const;
    
    /**
     * @brief 检查交易是否属于本分片
     * @param targetShardId 交易目标分片ID
     * @return true 如果交易属于本分片
     */
    bool isInTargetShard(int targetShardId) const;
    
    // ========================================================================
    // COMPONENT INSTANCES
    // ========================================================================
    
    RegionalShardManager* shardManager_;  // 🔧 Pointer to global module's manager
    std::unique_ptr<HotStuffEngine> consensusEngine_;
    std::unique_ptr<VRMManager> reputationManager_;
    
    // ========================================================================
    // CONSENSUS GROUP MANAGEMENT (🆕 P1)
    // ========================================================================
    
    /**
     * @brief 触发共识群组选举
     */
    void electConsensusGroup();
    
    /**
     * @brief 检查是否需要重新选举
     */
    bool needsReelection() const;
    
    /**
     * @brief 获取当前epoch（区块数/epochBlocks）
     */
    int getCurrentEpoch() const;
    
    /**
     * @brief 检查节点是否应参与共识
     */
    bool shouldParticipateInConsensus() const;
    
    // ========================================================================
    // STATE VARIABLES
    // ========================================================================
    
    NodeID nodeID_;
    ShardID currentShardID_;
    bool isLeaderNode_;
    bool isInitialized_;
    
    // 🆕 共识群组相关
    NodeRole nodeRole_;              // 节点角色
    int lastElectionEpoch_;          // 上次选举的epoch
    int committedBlockCount_;        // 已提交区块数
    int epochBlocks_;                // 每轮选举的区块数（默认10）
    
    // Transaction pool
    std::vector<tribft::Transaction> txPool_;
    int txCounter_;
    
    // 🆕 多跳转发相关
    std::set<std::string> seenTxIds_;  // 已见过的交易ID，防止循环转发
    int maxHops_;                       // 最大跳数限制（默认3）
    bool enableMultiHop_;               // 是否启用多跳转发
    
    // ========================================================================
    // TIMERS
    // ========================================================================
    
    cMessage* consensusTimer_;
    cMessage* shardMaintenanceTimer_;
    cMessage* reputationDecayTimer_;
    cMessage* heartbeatTimer_;
    cMessage* txGenerationTimer_;  // 🆕 交易生成定时器
    
    // ========================================================================
    // PARAMETERS (from NED)
    // ========================================================================
    
    simtime_t blockInterval_;
    int batchSize_;
    simtime_t consensusTimeout_;
    bool vrmEnabled_;
    double initialReputation_;
    
    // 🆕 自动交易生成参数
    bool autoGenerateTx_;
    simtime_t txGenerationInterval_;
    
    // ========================================================================
    // STATISTICS SIGNALS
    // ========================================================================
    
    simsignal_t blockCommittedSignal_;
    simsignal_t consensusLatencySignal_;
    simsignal_t reputationSignal_;
    simsignal_t throughputSignal_;
    simsignal_t shardSizeSignal_;
};

Define_Module(TriBFTApp);

} // namespace tribft

#endif // TRIBFT_APP_H

