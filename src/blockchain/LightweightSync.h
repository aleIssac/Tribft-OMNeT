#ifndef LIGHTWEIGHT_SYNC_H
#define LIGHTWEIGHT_SYNC_H

#include <map>
#include <vector>
#include <functional>
#include "../common/TriBFTDefs.h"
#include "../consensus/VRFSelector.h"  // For NodeRole

namespace tribft {

/**
 * @brief Block header (lightweight)
 */
struct BlockHeader {
    BlockHeight height;
    std::string blockHash;
    std::string previousHash;
    std::string merkleRoot;        // Transaction Merkle tree root
    ShardID shardID;
    simtime_t timestamp;
    NodeID proposer;
    int txCount;                   // Transaction count
    
    BlockHeader() : height(0), shardID(-1), timestamp(0), txCount(0) {}
    
    /**
     * @brief Extract block header from full block
     */
    static BlockHeader fromBlock(const Block& block) {
        BlockHeader header;
        header.height = block.height;
        header.blockHash = block.blockHash;
        header.previousHash = block.previousHash;
        header.merkleRoot = calculateMerkleRoot(block.transactions);
        header.shardID = block.shardID;
        header.timestamp = block.timestamp;
        header.proposer = block.proposer;
        header.txCount = block.transactions.size();
        return header;
    }
    
    /**
     * @brief Calculate Merkle root (simplified)
     */
    static std::string calculateMerkleRoot(const std::vector<Transaction>& txs) {
        if (txs.empty()) {
            return "EMPTY_ROOT";
        }
        
        // Simplified: concatenate all transaction IDs
        std::string combined;
        for (const auto& tx : txs) {
            combined += tx.txID;
        }
        
        std::hash<std::string> hasher;
        size_t hashValue = hasher(combined);
        return "MERKLE_" + std::to_string(hashValue);
    }
};

/**
 * @brief Merkle proof (for verifying single transaction)
 */
struct MerkleProof {
    std::string txHash;                    // Transaction hash
    std::vector<std::string> siblings;     // Sibling node hashes
    std::vector<bool> directions;          // Directions (left=0, right=1)
    
    MerkleProof() {}
};

/**
 * @brief Lightweight sync manager
 * 
 * Features:
 * - Ordinary nodes only sync block headers (reduce storage)
 * - Download full transactions on demand (reduce bandwidth)
 * - Merkle tree verification (ensure security)
 * 
 * Design Principles:
 * - KISS: Simplified SPV (Simplified Payment Verification)
 * - Storage optimization: only save headers (~100 bytes vs full block ~10KB)
 * - On-demand loading: only download needed transactions
 */
class LightweightSync {
public:
    using LogCallback = std::function<void(const std::string&)>;
    using RequestCallback = std::function<void(const std::string&, BlockHeight)>;
    
    LightweightSync();
    ~LightweightSync() = default;
    
    // ========================================================================
    // Initialization
    // ========================================================================
    
    void initialize(NodeRole role);
    void setLogCallback(LogCallback callback);
    void setRequestCallback(RequestCallback callback);
    
    // ========================================================================
    // Block Header Management
    // ========================================================================
    
    /**
     * @brief Sync block header
     * @param header Received block header
     * @return Whether successfully added
     */
    bool syncHeader(const BlockHeader& header);
    
    /**
     * @brief Get block header
     */
    const BlockHeader* getHeader(BlockHeight height) const;
    
    /**
     * @brief Get latest block height
     */
    BlockHeight getLatestHeight() const { return latestHeight_; }
    
    /**
     * @brief Check if block header exists
     */
    bool hasHeader(BlockHeight height) const;
    
    // ========================================================================
    // Full Block Management (on-demand loading)
    // ========================================================================
    
    /**
     * @brief Request to download full block
     * @param height Block height
     * @return Request ID
     */
    std::string requestFullBlock(BlockHeight height);
    
    /**
     * @brief Receive full block
     * @param block Full block
     * @return Whether verification passed
     */
    bool receiveFullBlock(const Block& block);
    
    /**
     * @brief Check if full block exists
     */
    bool hasFullBlock(BlockHeight height) const;
    
    /**
     * @brief Get full block
     */
    const Block* getFullBlock(BlockHeight height) const;
    
    // ========================================================================
    // Transaction Verification
    // ========================================================================
    
    /**
     * @brief Verify transaction is in block (using Merkle proof)
     * @param height Block height
     * @param txHash Transaction hash
     * @param proof Merkle proof
     * @return Verification result
     */
    bool verifyTransaction(
        BlockHeight height,
        const std::string& txHash,
        const MerkleProof& proof
    ) const;
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /**
     * @brief Get storage statistics
     */
    struct StorageStats {
        int headerCount;
        int fullBlockCount;
        size_t headerStorage;      // bytes
        size_t fullBlockStorage;   // bytes
        double compressionRatio;   // compression ratio
    };
    
    StorageStats getStorageStats() const;
    
    /**
     * @brief Cleanup old data (keep latest N blocks)
     */
    void cleanup(int keepCount = 100);
    
private:
    // ========================================================================
    // Internal Methods
    // ========================================================================
    
    /**
     * @brief Validate block header chain
     */
    bool validateHeaderChain(const BlockHeader& header) const;
    
    /**
     * @brief Generate request ID
     */
    std::string generateRequestID(BlockHeight height);
    
    /**
     * @brief Log output
     */
    void log(const std::string& message) const;
    
    // ========================================================================
    // Data Members
    // ========================================================================
    
    NodeRole nodeRole_;
    
    // Block header storage (all nodes)
    std::map<BlockHeight, BlockHeader> headers_;
    BlockHeight latestHeight_;
    
    // Full block storage (on-demand, limited nodes only)
    std::map<BlockHeight, Block> fullBlocks_;
    
    // Request tracking
    std::map<std::string, BlockHeight> pendingRequests_;
    
    LogCallback logCallback_;
    RequestCallback requestCallback_;
};

} // namespace tribft

#endif // LIGHTWEIGHT_SYNC_H

