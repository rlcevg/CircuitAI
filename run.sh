#!/bin/sh
make CircuitAI -j8
cp AI/Skirmish/CircuitAI/libSkirmishAI.so ~/.spring/engine/101.0-test/AI/Skirmish/CircuitAI/0.8.4/
echo "Done!"
