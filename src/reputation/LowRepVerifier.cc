#include "LowRepVerifier.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace tribft {

LowRepVerifier::LowRepVerifier()
    : verifiersPerEvent_(3)
    , threshold_(0.67)
{
}

void LowRepVerifier::initialize(int verifiersPerEvent, double threshold) {
    verifiersPerEvent_ = verifiersPerEvent;
    threshold_ = threshold;
    log("LowRepVerifier initialized (verifiers=" + std::to_string(verifiersPerEvent) +
        ", threshold=" + std::to_string(threshold) + ")");
}

void LowRepVerifier::setLogCallback(LogCallback callback) {
    logCallback_ = callback;
}

void LowRepVerifier::setVerificationCallback(VerificationCallback callback) {
    verificationCallback_ = callback;
}

// ============================================================================
// Event Management
// ============================================================================

std::string LowRepVerifier::submitEvent(
    const NodeID& reporterID,
    const std::string& eventType,
    const std::string& eventData,
    double reporterRep)
{
    simtime_t now = simTime();
    std::string eventID = generateEventID(reporterID, now);
    
    PendingEvent event;
    event.reporterID = reporterID;
    event.eventID = eventID;
    event.eventType = eventType;
    event.eventData = eventData;
    event.timestamp = now;
    event.reporterReputation = reporterRep;
    
    pendingEvents_[eventID] = event;
    
    log("Event submitted: " + eventID + " from " + reporterID + 
        " (rep=" + std::to_string(reporterRep) + ")");
    
    return eventID;
}

std::vector<NodeID> LowRepVerifier::assignVerifiers(
    const std::string& eventID,
    const std::vector<NodeID>& trustedNodes,
    uint64_t seed)
{
    auto it = pendingEvents_.find(eventID);
    if (it == pendingEvents_.end()) {
        return {};
    }
    
    // Filter out the reporter itself
    std::vector<NodeID> candidates;
    for (const auto& nodeID : trustedNodes) {
        if (nodeID != it->second.reporterID) {
            candidates.push_back(nodeID);
        }
    }
    
    // Select verifiers using VRF
    std::vector<NodeID> verifiers = selectVerifiers(candidates, verifiersPerEvent_, seed);
    
    // Record task
    VerificationTask task;
    task.eventID = eventID;
    task.verifiers = verifiers;
    task.assignedTime = simTime();
    tasks_[eventID] = task;
    
    log("Verifiers assigned for " + eventID + ": " + std::to_string(verifiers.size()) + " nodes");
    
    return verifiers;
}

void LowRepVerifier::submitVerification(
    const std::string& eventID,
    const NodeID& verifierID,
    bool confirm)
{
    auto it = pendingEvents_.find(eventID);
    if (it == pendingEvents_.end()) {
        log("ERROR: Event " + eventID + " not found");
        return;
    }
    
    PendingEvent& event = it->second;
    event.verificationCount++;
    
    if (confirm) {
        event.confirmCount++;
    } else {
        event.rejectCount++;
    }
    
    log("Verification from " + verifierID + " for " + eventID + ": " +
        (confirm ? "CONFIRM" : "REJECT") + " (" + 
        std::to_string(event.confirmCount) + "/" + 
        std::to_string(event.rejectCount) + ")");
    
    // Check if verification threshold is reached
    if (checkVerificationThreshold(event)) {
        // Verification complete
        event.verified = true;
        double confirmRatio = static_cast<double>(event.confirmCount) / 
                             event.verificationCount;
        event.result = (confirmRatio >= threshold_);
        
        log(">>>VERIFICATION_COMPLETE<<< Event " + eventID + ": " +
            (event.result ? "TRUE" : "FALSE") + 
            " (ratio=" + std::to_string(confirmRatio) + ")");
        
        // Callback notification
        if (verificationCallback_) {
            verificationCallback_(eventID, event.result);
        }
        
        // Cleanup verified events (optional: keep for statistics)
        // pendingEvents_.erase(it);
        // tasks_.erase(eventID);
    }
}

bool LowRepVerifier::isEventVerified(const std::string& eventID) const {
    auto it = pendingEvents_.find(eventID);
    if (it != pendingEvents_.end()) {
        return it->second.verified;
    }
    return false;
}

bool LowRepVerifier::getVerificationResult(const std::string& eventID) const {
    auto it = pendingEvents_.find(eventID);
    if (it != pendingEvents_.end() && it->second.verified) {
        return it->second.result;
    }
    return false;
}

void LowRepVerifier::cleanupExpiredEvents(simtime_t currentTime, double timeout) {
    std::vector<std::string> toRemove;
    
    for (const auto& pair : pendingEvents_) {
        const PendingEvent& event = pair.second;
        if (!event.verified && (currentTime - event.timestamp).dbl() > timeout) {
            toRemove.push_back(pair.first);
        }
    }
    
    for (const auto& eventID : toRemove) {
        log("Cleanup expired event: " + eventID);
        pendingEvents_.erase(eventID);
        tasks_.erase(eventID);
    }
}

// ============================================================================
// Internal Methods
// ============================================================================

std::string LowRepVerifier::generateEventID(const NodeID& reporterID, simtime_t timestamp) {
    std::ostringstream oss;
    oss << "EVT_" << reporterID << "_" << std::fixed << std::setprecision(3) << timestamp.dbl();
    return oss.str();
}

std::vector<NodeID> LowRepVerifier::selectVerifiers(
    const std::vector<NodeID>& trustedNodes,
    int count,
    uint64_t seed)
{
    if (trustedNodes.empty()) {
        return {};
    }
    
    // Simplified VRF: hash-based sorting
    std::vector<std::pair<size_t, NodeID>> scored;
    std::hash<std::string> hasher;
    
    for (const auto& nodeID : trustedNodes) {
        std::string input = nodeID + std::to_string(seed);
        size_t score = hasher(input);
        scored.push_back({score, nodeID});
    }
    
    // Sort by score
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });
    
    // Select top N
    std::vector<NodeID> selected;
    int selectCount = std::min(count, static_cast<int>(scored.size()));
    for (int i = 0; i < selectCount; ++i) {
        selected.push_back(scored[i].second);
    }
    
    return selected;
}

bool LowRepVerifier::checkVerificationThreshold(const PendingEvent& event) const {
    // At least received required number of verifier responses
    if (event.verificationCount < verifiersPerEvent_) {
        return false;
    }
    
    // Or result can already be determined (early termination)
    // e.g., 3 verifiers, 2 already confirmed, will pass regardless of 3rd
    double confirmRatio = static_cast<double>(event.confirmCount) / event.verificationCount;
    double rejectRatio = static_cast<double>(event.rejectCount) / event.verificationCount;
    
    // Confirmation reaches threshold or rejection exceeds (1-threshold)
    if (confirmRatio >= threshold_ || rejectRatio > (1.0 - threshold_)) {
        return true;
    }
    
    return false;
}

void LowRepVerifier::log(const std::string& message) const {
    if (logCallback_) {
        logCallback_("[LowRepVerifier] " + message);
    }
}

} // namespace tribft

