#!/bin/sh
make CircuitAI -j8
cp AI/Skirmish/CircuitAI/libSkirmishAI.so ~/.spring/engine/103.0-test/AI/Skirmish/CircuitAI/0.9.7/
echo "Done!"
