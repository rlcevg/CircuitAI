#include "side.as"
#include "role.as"


namespace Init {

void Init(dictionary@ data)
{
	aiLog("AngelScript Rules!");

	dictionary category;
	category["air"]   = "VTOL NOTSUB";
	category["land"]  = "SURFACE NOTSUB";
	category["water"] = "UNDERWATER NOTHOVER";
	category["bad"]   = "TERRAFORM STUPIDTARGET MINE";
	category["good"]  = "TURRET FLOAT";
	data["category"] = @category;

	dictionary profile;
	profile["default"] = @(array<string> = {"behaviour", "block_map", "build_chain", "commander", "economy", "factory", "response"});
	profile["easy"] = @(array<string> = {"behaviour", "block_map", "build_chain", "commander", "economy", "factory", "response"});
	profile["dev"] = @(array<string> = {"behaviour", "block_map", "build_chain", "commander", "economy", "factory", "response"});
	profile["flaka"] = @(array<string> = {"behaviour", "block_map", "build_chain", "commander", "economy", "factory", "response"});
	data["profile"] = @profile;
}

}
