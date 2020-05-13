#include "role.as"


namespace Init {

void Init(dictionary@ categories)
{
	aiLog("AngelScript Rules!");

	categories["air"]   = "FIXEDWING GUNSHIP";
	categories["land"]  = "LAND SINK TURRET SHIP SWIM FLOAT HOVER";
	categories["water"] = "SUB";
	categories["bad"]   = "TERRAFORM STUPIDTARGET MINE";
	categories["good"]  = "TURRET FLOAT";
}

}
