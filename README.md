# TriBFT-OMNeT++

A Byzantine Fault Tolerant (BFT) consensus protocol simulation for Internet of Vehicles (IoV) based on OMNeT++ and Veins.

## Overview

TriBFT is a reputation-based BFT consensus protocol designed for vehicular networks. This project implements the TriBFT protocol in the OMNeT++ discrete event simulation framework with Veins for vehicular network simulation.

## Simulation Environment

### Software Dependencies

| Software | Version |
|----------|---------|
| SUMO (Simulation of Urban Mobility) | 1.21 |
| OMNeT++ | 6.2 |
| Veins | 5.3.1 |

### Simulation Map

The simulation uses a real-world road network from **Nanning City, Guangxi, China**, specifically covering parts of **Qingxiu District** and **Jiangnan District**.

**Map Specifications:**
- **East-West Span:** 9.64 km
- **North-South Span:** 5.84 km
- **Total Area:** ~56.32 km²

## Project Structure

```
Tribft-OMNeT/
├── src/
│   ├── application/      # TriBFT application layer
│   ├── blockchain/       # Lightweight blockchain sync
│   ├── common/           # Common definitions and types
│   ├── consensus/        # HotStuff consensus engine & VRF selector
│   ├── messages/         # OMNeT++ message definitions
│   ├── nodes/            # Node definitions
│   ├── reputation/       # Vehicle Reputation Management (VRM)
│   └── shard/            # Regional shard management
├── simulations/
│   └── veins-base/       # Simulation configurations and scenarios
└── Makefile
```

## Key Features

- **HotStuff-based BFT Consensus:** Three-phase consensus protocol (PREPARE, PRE-COMMIT, COMMIT)
- **VRF-based Leader Election:** Verifiable Random Function for fair leader selection
- **Dual Reputation Model:** Global and local reputation scoring system
- **Regional Sharding:** Geographic-based shard management for scalability
- **Lightweight Sync:** Block header synchronization for ordinary nodes

## Building

```bash
# Configure and build
make makefiles
make
```

## Running Simulations

```bash
cd simulations/veins-base
# Start SUMO launcher first
./run
```

## License

This project is for research purposes.

## Note

> Core implementation details are hidden and will be released after project completion.
