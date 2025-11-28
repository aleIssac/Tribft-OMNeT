#ifndef LOW_REP_VERIFIER_H
#define LOW_REP_VERIFIER_H

#include <map>
#include <vector>
#include <queue>
#include <functional>
#include "../common/TriBFTDefs.h"

namespace tribft {

/**
 * @brief Pending verification event
 */
struct PendingEvent {
    NodeID reporterID;          // Reporter ID
    std::string eventID;        // Event ID
    std::string eventType;      // Event type
    std::string eventData;      // Event data
    simtime_t timestamp;        // Submission time
    double reporterReputation;  // Reporter's reputation
    
    // Verification status
    int verificationCount;      // Number of verifications received
    int confirmCount;           // Number of confirmations
    int rejectCount;            // Number of rejections
    bool verified;              // Whether verification is complete
    bool result;                // Verification result (true=authentic, false=false)
    
    PendingEvent() : timestamp(0), reporterReputation(0.0),
                     verificationCount(0), confirmCount(0), 
                     rejectCount(0), verified(false), result(false) {}
};

/**
 * @brief Verification task
 */
struct VerificationTask {
    std::string eventID;
    std::vector<NodeID> verifiers;  // Selected verifiers
    simtime_t assignedTime;         // Assignment time
    
    VerificationTask() : assignedTime(0) {}
};

/**
 * @brief Low reputation node verifier
 * 
 * Features:
 * - Manage pending event pool
 * - Select high-reputation verifiers (using VRF)
 * - Collect verification results
 * - Feedback to reputation system
 * 
 * Paper mechanism:
 * - Events from low-rep nodes (R<0.2) require cross-verification
 * - Randomly select K verifiers from high-rep nodes (R>=0.8)
 * - Majority voting determines event authenticity
 * - False reports receive severe penalties
 */
class LowRepVerifier {
public:
    using LogCallback = std::function<void(const std::string&)>;
    using VerificationCallback = std::function<void(const std::string&, bool)>;
    
    LowRepVerifier();
    ~LowRepVerifier() = default;
    
    // ========================================================================
    // Initialization
    // ========================================================================
    
    void initialize(int verifiersPerEvent = 3, double threshold = 0.67);
    void setLogCallback(LogCallback callback);
    void setVerificationCallback(VerificationCallback callback);
    
    // ========================================================================
    // Event Management
    // ========================================================================
    
    /**
     * @brief Submit event for verification
     * @param reporterID Reporter's node ID
     * @param eventType Event type (e.g., "TRANSACTION")
     * @param eventData Event data
     * @param reporterRep Reporter's current reputation
     * @return Event ID
     */
    std::string submitEvent(
        const NodeID& reporterID,
        const std::string& eventType,
        const std::string& eventData,
        double reporterRep
    );
    
    /**
     * @brief Assign verification task
     * @param eventID Event ID
     * @param trustedNodes List of trusted nodes (R>=0.8)
     * @param seed VRF seed
     * @return List of selected verifiers
     */
    std::vector<NodeID> assignVerifiers(
        const std::string& eventID,
        const std::vector<NodeID>& trustedNodes,
        uint64_t seed
    );
    
    /**
     * @brief Submit verification result
     * @param eventID Event ID
     * @param verifierID Verifier's node ID
     * @param confirm Whether to confirm event authenticity
     */
    void submitVerification(
        const std::string& eventID,
        const NodeID& verifierID,
        bool confirm
    );
    
    /**
     * @brief Check if event verification is complete
     */
    bool isEventVerified(const std::string& eventID) const;
    
    /**
     * @brief Get event verification result
     * @return true=authentic, false=false report
     */
    bool getVerificationResult(const std::string& eventID) const;
    
    /**
     * @brief Get pending event count
     */
    int getPendingCount() const { return pendingEvents_.size(); }
    
    /**
     * @brief Cleanup expired events (verification timeout)
     */
    void cleanupExpiredEvents(simtime_t currentTime, double timeout = 10.0);
    
private:
    // ========================================================================
    // Internal Methods
    // ========================================================================
    
    /**
     * @brief Generate event ID
     */
    std::string generateEventID(const NodeID& reporterID, simtime_t timestamp);
    
    /**
     * @brief Select verifiers using VRF
     */
    std::vector<NodeID> selectVerifiers(
        const std::vector<NodeID>& trustedNodes,
        int count,
        uint64_t seed
    );
    
    /**
     * @brief Check if verification threshold is reached
     */
    bool checkVerificationThreshold(const PendingEvent& event) const;
    
    /**
     * @brief Log output
     */
    void log(const std::string& message) const;
    
    // ========================================================================
    // Data Members
    // ========================================================================
    
    std::map<std::string, PendingEvent> pendingEvents_;    // Pending event pool
    std::map<std::string, VerificationTask> tasks_;        // Verification tasks
    
    int verifiersPerEvent_;     // Verifiers per event (default 3)
    double threshold_;          // Verification threshold (default 0.67, i.e., 2/3 majority)
    
    LogCallback logCallback_;
    VerificationCallback verificationCallback_;
};

} // namespace tribft

#endif // LOW_REP_VERIFIER_H

