#include "../common.as"
#include "../default/side.as"
#include "../default/role.as"


namespace Init {

void Init(dictionary@ data)
{
	aiLog("flaka AngelScript Rules!");

	data["category"] = @InitCategories();
	data["profile"] = @(array<string> = {"behaviour", "block_map", "build_chain", "commander", "economy", "factory", "response"});
}

}
