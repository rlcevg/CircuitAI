#include "../common.as"
#include "role.as"


namespace Init {

SInitInfo AiInit()
{
	AiLog("default AngelScript Rules!");

	SInitInfo data;
	data.category = InitCategories();
	@data.profile = @(array<string> = {"behaviour", "block_map", "build_chain", "commander", "economy", "factory", "response"});
	return data;
}

}
