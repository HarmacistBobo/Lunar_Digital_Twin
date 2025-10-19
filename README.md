# Lunar Digital Twin (LDT) Simulation

This repository contains only the relevant simulation source and configuration files for the Lunar Digital Twin project built using ns-3.45.

## Directory Structure
- `/scratch` — main simulation source files (`*.cc`)
- `/scratch/config` — configuration templates (`*.txt`)
- `/scratch/config/LTE_config` — LTE configuration files (`*.conf`)
- `/scratch_helpers` — supporting helper classes and headers (`*.cc`, `*.h`)

## Usage
1. Place these files into the corresponding directories in your ns-3.45 installation.
2. Build and run:
   ```bash
   ./ns3 run scratch/LDT_main.cc

