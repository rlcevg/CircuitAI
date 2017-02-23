#!/bin/sh
make CircuitAI -j8
cp AI/Skirmish/CircuitAI/data/libSkirmishAI.so ~/.spring/engine/103.0-test/AI/Skirmish/CircuitAI/dev/
echo "Done!"
