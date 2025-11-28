#include "LightweightSync.h"
#include <sstream>

namespace tribft {

LightweightSync::LightweightSync()
    : nodeRole_(NodeRole::ORDINARY)
    , latestHeight_(0)
{
}

void LightweightSync::initialize(NodeRole role) {
    nodeRole_ = role;
    latestHeight_ = 0;
    headers_.clear();
    fullBlocks_.clear();
    
    std::string roleStr;
    switch (role) {
        case NodeRole::ORDINARY: roleStr = "ORDINARY"; break;
        case NodeRole::CONSENSUS_PRIMARY: roleStr = "PRIMARY"; break;
        case NodeRole::CONSENSUS_REDUNDANT: roleStr = "REDUNDANT"; break;
        case NodeRole::RSU_PERMANENT: roleStr = "RSU"; break;
    }
    
    log("LightweightSync initialized (role=" + roleStr + ")");
}

void LightweightSync::setLogCallback(LogCallback callback) {
    logCallback_ = callback;
}

void LightweightSync::setRequestCallback(RequestCallback callback) {
    requestCallback_ = callback;
}

// ============================================================================
// Block Header Management
// ============================================================================

bool LightweightSync::syncHeader(const BlockHeader& header) {
    // Validate block header chain
    if (!validateHeaderChain(header)) {
        log("Header validation failed for height " + std::to_string(header.height));
        return false;
    }
    
    // Store block header
    headers_[header.height] = header;
    
    // Update latest height
    if (header.height > latestHeight_) {
        latestHeight_ = header.height;
    }
    
    log(">>>HEADER_SYNCED<<< Height: " + std::to_string(header.height) +
        ", TxCount: " + std::to_string(header.txCount) +
        ", Proposer: " + header.proposer);
    
    return true;
}

const BlockHeader* LightweightSync::getHeader(BlockHeight height) const {
    auto it = headers_.find(height);
    if (it != headers_.end()) {
        return &(it->second);
    }
    return nullptr;
}

bool LightweightSync::hasHeader(BlockHeight height) const {
    return headers_.find(height) != headers_.end();
}

// ============================================================================
// Full Block Management
// ============================================================================

std::string LightweightSync::requestFullBlock(BlockHeight height) {
    std::string requestID = generateRequestID(height);
    pendingRequests_[requestID] = height;
    
    log(">>>FULL_BLOCK_REQUEST<<< Height: " + std::to_string(height) +
        ", RequestID: " + requestID);
    
    // Trigger callback (upper layer sends network request)
    if (requestCallback_) {
        requestCallback_(requestID, height);
    }
    
    return requestID;
}

bool LightweightSync::receiveFullBlock(const Block& block) {
    // Check if corresponding block header exists
    if (!hasHeader(block.height)) {
        log("No header for full block at height " + std::to_string(block.height));
        return false;
    }
    
    const BlockHeader* header = getHeader(block.height);
    
    // Verify block hash
    if (header->blockHash != block.blockHash) {
        log("Block hash mismatch at height " + std::to_string(block.height));
        return false;
    }
    
    // Verify Merkle root
    std::string merkleRoot = BlockHeader::calculateMerkleRoot(block.transactions);
    if (header->merkleRoot != merkleRoot) {
        log("Merkle root mismatch at height " + std::to_string(block.height));
        return false;
    }
    
    // Verify transaction count
    if (header->txCount != static_cast<int>(block.transactions.size())) {
        log("Transaction count mismatch at height " + std::to_string(block.height));
        return false;
    }
    
    // Store full block
    fullBlocks_[block.height] = block;
    
    log(">>>FULL_BLOCK_RECEIVED<<< Height: " + std::to_string(block.height) +
        ", TxCount: " + std::to_string(block.transactions.size()) +
        ", Verified: YES");
    
    return true;
}

bool LightweightSync::hasFullBlock(BlockHeight height) const {
    return fullBlocks_.find(height) != fullBlocks_.end();
}

const Block* LightweightSync::getFullBlock(BlockHeight height) const {
    auto it = fullBlocks_.find(height);
    if (it != fullBlocks_.end()) {
        return &(it->second);
    }
    return nullptr;
}

// ============================================================================
// Transaction Verification
// ============================================================================

bool LightweightSync::verifyTransaction(
    BlockHeight height,
    const std::string& txHash,
    const MerkleProof& proof) const
{
    // Get block header
    const BlockHeader* header = getHeader(height);
    if (!header) {
        return false;
    }
    
    // Simplified Merkle verification
    // In production, full Merkle tree verification should be implemented
    std::string computedRoot = txHash;
    
    for (size_t i = 0; i < proof.siblings.size(); ++i) {
        std::hash<std::string> hasher;
        if (proof.directions[i]) {
            // Sibling on left
            computedRoot = std::to_string(hasher(proof.siblings[i] + computedRoot));
        } else {
            // Sibling on right
            computedRoot = std::to_string(hasher(computedRoot + proof.siblings[i]));
        }
    }
    
    return (computedRoot == header->merkleRoot);
}

// ============================================================================
// Statistics
// ============================================================================

LightweightSync::StorageStats LightweightSync::getStorageStats() const {
    StorageStats stats;
    stats.headerCount = headers_.size();
    stats.fullBlockCount = fullBlocks_.size();
    
    // Estimate storage size
    stats.headerStorage = headers_.size() * 200;  // Assume 200 bytes per header
    
    stats.fullBlockStorage = 0;
    for (const auto& pair : fullBlocks_) {
        stats.fullBlockStorage += pair.second.transactions.size() * 500;  // Assume 500 bytes per tx
    }
    
    // Calculate compression ratio
    if (stats.headerStorage + stats.fullBlockStorage > 0) {
        stats.compressionRatio = static_cast<double>(stats.headerStorage) /
                                 (stats.headerStorage + stats.fullBlockStorage);
    } else {
        stats.compressionRatio = 1.0;
    }
    
    return stats;
}

void LightweightSync::cleanup(int keepCount) {
    if (latestHeight_ <= keepCount) {
        return;
    }
    
    BlockHeight cutoff = latestHeight_ - keepCount;
    
    // Cleanup old block headers
    auto it = headers_.begin();
    while (it != headers_.end()) {
        if (it->first < cutoff) {
            it = headers_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Cleanup old full blocks
    auto it2 = fullBlocks_.begin();
    while (it2 != fullBlocks_.end()) {
        if (it2->first < cutoff) {
            it2 = fullBlocks_.erase(it2);
        } else {
            ++it2;
        }
    }
    
    log("Cleanup complete. Kept last " + std::to_string(keepCount) + " blocks");
}

// ============================================================================
// Internal Methods
// ============================================================================

bool LightweightSync::validateHeaderChain(const BlockHeader& header) const {
    // Genesis block
    if (header.height == 0) {
        return true;
    }
    
    // Check if previous block header exists
    const BlockHeader* prevHeader = getHeader(header.height - 1);
    if (!prevHeader) {
        // If this is the first block header, allow
        if (headers_.empty()) {
            return true;
        }
        log("Previous header not found for height " + std::to_string(header.height));
        return false;
    }
    
    // Verify previous hash
    if (header.previousHash != prevHeader->blockHash) {
        log("Previous hash mismatch at height " + std::to_string(header.height));
        return false;
    }
    
    // Verify height increment
    if (header.height != prevHeader->height + 1) {
        log("Height not incremental at height " + std::to_string(header.height));
        return false;
    }
    
    return true;
}

std::string LightweightSync::generateRequestID(BlockHeight height) {
    std::ostringstream oss;
    oss << "REQ_" << height << "_" << simTime().dbl();
    return oss.str();
}

void LightweightSync::log(const std::string& message) const {
    if (logCallback_) {
        logCallback_("[LightweightSync] " + message);
    }
}

} // namespace tribft

