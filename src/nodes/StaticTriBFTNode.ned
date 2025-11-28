package tribft.nodes;

import org.car2x.veins.base.modules.BaseMobility;
import org.car2x.veins.modules.nic.Nic80211p;
import tribft.application.TriBFTApp;

module StaticTriBFTNode
{
    parameters:
        @display("i=block/network2");
        
    gates:
        input radioIn; // gate for receiving messages from network layer
        
    submodules:
        appl: TriBFTApp {
            @display("p=60,50");
        }
        
        nic: Nic80211p {
            @display("p=60,166");
        }
        
        mobility: BaseMobility {
            @display("p=130,172;i=block/cogwheel");
        }
        
    connections:
        nic.upperLayerOut --> appl.lowerLayerIn;
        nic.upperLayerIn <-- appl.lowerLayerOut;
        nic.upperControlOut --> appl.lowerControlIn;
        nic.upperControlIn <-- appl.lowerControlOut;
        
        radioIn --> nic.radioIn;
}

