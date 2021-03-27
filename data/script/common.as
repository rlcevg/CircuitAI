namespace Side {

/*
 * Register factions
 */
TypeMask ARMADA = aiSideMasker.GetTypeMask("armada");
TypeMask CORTEX = aiSideMasker.GetTypeMask("cortex");

}  // namespace Side

namespace Init {

SCategoryInfo InitCategories()
{
	SCategoryInfo category;
	category.air   = "VTOL NOTSUB";
	category.land  = "SURFACE NOTSUB";
	category.water = "UNDERWATER NOTHOVER";
	category.bad   = "TERRAFORM STUPIDTARGET MINE";
	category.good  = "TURRET FLOAT";
	return category;
}

SArmorInfo InitArmordef()
{
	// NOTE: Intentionally unsorted as it is in bar.sdd/gamedata/armordefs.lua
	//       Replicates engine's string<=>int assignment
	//       Must not include "default" keyword
	array<string> armors = {
		"commanders",
		"scavboss",
		"indestructable",
		"crawlingbombs",
		"standard",
		"bombers",
		"fighters",
		"mines",
		"nanos",
		"vtol",
		"shields",
		"lboats",
		"hvyboats",
		"subs",
		"tinychicken",
		"chicken"
	};
	armors.sortAsc();
	armors.insertAt(0, "default");

	dictionary armorTypes;
	AiLog("Armordefs:");
	for (uint i = 0; i < armors.length(); ++i) {
		armorTypes[armors[i]] = i;
		AiLog(armors[i] + " = " + i);
	}

	array<array<string>> airGroups = {{"bombers", "fighters", "vtol"}};
	array<array<string>> surfaceGroups = {{"default"}, {"hvyboats"}};  // TODO: Remove hvyboats - has little impact
	array<array<string>> waterGroups = {{"subs"}};

	SArmorInfo armor;
	for (uint i = 0; i < airGroups.length(); ++i) {
		for (uint j = 0; j < airGroups[i].length(); ++j) {
			armor.AddAir(i, int(armorTypes[airGroups[i][j]]));
		}
	}
	for (uint i = 0; i < surfaceGroups.length(); ++i) {
		for (uint j = 0; j < surfaceGroups[i].length(); ++j) {
			armor.AddSurface(i, int(armorTypes[surfaceGroups[i][j]]));
		}
	}
	for (uint i = 0; i < waterGroups.length(); ++i) {
		for (uint j = 0; j < waterGroups[i].length(); ++j) {
			armor.AddWater(i, int(armorTypes[waterGroups[i][j]]));
		}
	}
	return armor;
}

}  // namespace Init
