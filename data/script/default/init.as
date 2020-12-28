#include "../common.as"
#include "side.as"
#include "role.as"


namespace Init {

void Init(dictionary@ data)
{
	aiLog("default AngelScript Rules!");

	data["category"] = InitCategories();
	data["profile"] = @(array<string> = {"behaviour", "block_map", "build_chain", "commander", "economy", "factory", "response"});
}

}
