package tribft.application;

// CRITICAL: Use full package name org.car2x.veins, NOT simplified veins
// This is required for Veins 5.3.1 compatibility
import org.car2x.veins.modules.application.ieee80211p.DemoBaseApplLayer;

simple TriBFTApp extends DemoBaseApplLayer
{
    parameters:
        @class(tribft::TriBFTApp);
        @display("i=block/app");
        
        // Consensus parameters
        double blockInterval @unit(s) = default(0.5s);  // Block generation interval
        int batchSize = default(10);                     // Transactions per block
        double consensusTimeout @unit(s) = default(2.0s); // Consensus timeout
        
        // Transaction generation parameters
        bool autoGenerateTx = default(false);            // Enable automatic transaction generation
        double txGenerationInterval @unit(s) = default(5.0s); // Transaction generation interval
        
        // Multi-hop forwarding parameters
        bool enableMultiHop = default(true);             // Enable multi-hop transaction forwarding
        int maxHops = default(3);                        // Maximum number of hops (1-4)
        
        // 🆕 RSU parameters
        bool isRSU = default(false);                     // Mark as RSU node (fixed position, priority as Leader)
        
        // VRM (Vehicle Reputation Management) parameters
        bool vrmEnabled = default(true);                 // Enable reputation system
        double initialReputation = default(0.5);         // Initial reputation score [0.0-1.0]
        
        // Statistics signals
        @signal[blockCommitted](type=long);
        @signal[consensusLatency](type=simtime_t);
        @signal[reputation](type=double);
        @signal[throughput](type=double);
        @signal[shardSize](type=long);
        
        // Statistics recording
        @statistic[blockCommitted](title="Blocks Committed"; record=count,sum,vector);
        @statistic[consensusLatency](title="Consensus Latency"; unit=s; record=stats,histogram,vector);
        @statistic[reputation](title="Reputation Score"; record=stats,vector);
        @statistic[throughput](title="Throughput (TPS)"; record=stats,histogram,vector);
        @statistic[shardSize](title="Shard Size"; record=stats,vector);
}


