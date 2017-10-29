#!/bin/sh
make CircuitAI -j8
cp AI/Skirmish/CircuitAI/data/libSkirmishAI.so ~/.spring/engine/104.0-test/AI/Skirmish/CircuitAI/stable/
echo "Done!"
