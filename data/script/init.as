#include "role.as"


namespace Init {

void Init(dictionary@ data)
{
	aiLog("AngelScript Rules!");

	dictionary category;
	category["air"]   = "FIXEDWING GUNSHIP";
	category["land"]  = "LAND SINK TURRET SHIP SWIM FLOAT HOVER";
	category["water"] = "SUB";
	category["bad"]   = "TERRAFORM STUPIDTARGET MINE";
	category["good"]  = "TURRET FLOAT";

	data["category"] = @category;
}

}
