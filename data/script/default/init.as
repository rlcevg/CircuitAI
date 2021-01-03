#include "../common.as"
#include "side.as"
#include "role.as"


namespace Init {

void AiInit(dictionary@ data)
{
	AiLog("default AngelScript Rules!");

	data["category"] = InitCategories();
	data["profile"] = @(array<string> = {"behaviour", "block_map", "build_chain", "commander", "economy", "factory", "response"});
}

}
