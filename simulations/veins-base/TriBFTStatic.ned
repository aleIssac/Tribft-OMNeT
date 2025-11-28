//
// Static TriBFT Network - Manual node creation without TraCI
// Uses stationary nodes at fixed positions
//

import org.car2x.veins.base.connectionManager.ConnectionManager;
import org.car2x.veins.base.modules.BaseWorldUtility;
import org.car2x.veins.modules.world.annotations.AnnotationManager;
import org.car2x.veins.modules.obstacle.ObstacleControl;
import org.car2x.veins.modules.application.ieee80211p.DemoBaseApplLayer;
import org.car2x.veins.base.modules.BaseMobility;
import org.car2x.veins.modules.nic.Nic80211p;
import tribft.application.TriBFTApp;

// Simplified stationary node without TraCI
module StaticNode
{
    parameters:
        @display("i=device/antennatower");
        
    gates:
        input radioIn;
        
    submodules:
        appl: TriBFTApp {
            @display("p=60,50");
        }
        
        nic: Nic80211p {
            @display("p=60,166");
        }
        
        mobility: BaseMobility {
            @display("p=130,172");
        }
        
    connections:
        nic.upperLayerOut --> appl.lowerLayerIn;
        nic.upperLayerIn <-- appl.lowerLayerOut;
        nic.upperControlOut --> appl.lowerControlIn;
        nic.upperControlIn <-- appl.lowerControlOut;
        
        radioIn --> nic.radioIn;
}

network TriBFTStatic
{
    parameters:
        double playgroundSizeX @unit(m) = default(2500m);
        double playgroundSizeY @unit(m) = default(2500m);
        double playgroundSizeZ @unit(m) = default(50m);
        @display("bgb=2500,2500");
        
    submodules:
        connectionManager: ConnectionManager {
            parameters:
                @display("p=150,0;i=abstract/multicast");
        }
        world: BaseWorldUtility {
            parameters:
                playgroundSizeX = parent.playgroundSizeX;
                playgroundSizeY = parent.playgroundSizeY;
                playgroundSizeZ = parent.playgroundSizeZ;
                @display("p=30,0;i=misc/globe");
        }
        
        // 5 static nodes for testing
        node[5]: StaticNode {
        }
    
    connections allowunconnected:
}

