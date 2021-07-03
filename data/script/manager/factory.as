#include "../role.as"


namespace Factory {

string factorycloak  ("factorycloak");
string factorygunship("factorygunship");
string factoryamph   ("factoryamph");
string factoryspider ("factoryspider");
string factoryveh    ("factoryveh");
string factoryhover  ("factoryhover");
string factoryplane  ("factoryplane");
string factorytank   ("factorytank");
string factoryjump   ("factoryjump");
string factoryshield ("factoryshield");
string factoryship   ("factoryship");
string striderhub    ("striderhub");

Id CLOAK   = ai.GetCircuitDef(factorycloak).GetId();
Id GUNSHIP = ai.GetCircuitDef(factorygunship).GetId();
Id AMPH    = ai.GetCircuitDef(factoryamph).GetId();
Id SPIDER  = ai.GetCircuitDef(factoryspider).GetId();
Id VEH     = ai.GetCircuitDef(factoryveh).GetId();
Id HOVER   = ai.GetCircuitDef(factoryhover).GetId();
Id PLANE   = ai.GetCircuitDef(factoryplane).GetId();
Id TANK    = ai.GetCircuitDef(factorytank).GetId();
Id JUMP    = ai.GetCircuitDef(factoryjump).GetId();
Id SHIELD  = ai.GetCircuitDef(factoryshield).GetId();
Id SHIP    = ai.GetCircuitDef(factoryship).GetId();
Id STRIDER = ai.GetCircuitDef(striderhub).GetId();

IUnitTask@ MakeTask(CCircuitUnit@ unit)
{
	return aiFactoryMgr.DefaultMakeTask(unit);
}

}  // namespace Factory
