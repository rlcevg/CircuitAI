#include "role.as"
#include "manager/factory.as"


namespace Commander {

string support("support");
string recon  ("recon");
string assault("assault");
string strike ("strike");

}


namespace Opener {

shared class SO {  // SOrder
	SO(Type r, uint c) {
		role = r;
		count = c;
	}
	SO(Type r) {
		role = r;
		count = 1;
	}
	SO() {}
	Type role;
	uint count;
}

shared class SQueue {
	SQueue(float w, array<SO>& in o) {
		weight = w;
		orders = o;
	}
	SQueue() {}
	float weight;
	array<SO> orders;
}

shared class SOpener {
	SOpener(dictionary f, array<SO>& in d) {
		factory = f;
		def = d;
	}
	dictionary factory;
	array<SO> def;
}

dictionary@ GetOpenInfo()
{
	return dictionary = {
		{Commander::support, SOpener({
			{Factory::factorycloak, array<SQueue> = {
				SQueue(0.9f, {SO(RT::RAIDER, 2), SO(RT::BUILDER), SO(RT::RAIDER, 2), SO(RT::BUILDER), SO(RT::RAIDER, 4)}),
				SQueue(0.1f, {SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::RIOT), SO(RT::BUILDER), SO(RT::RAIDER, 4), SO(RT::BUILDER), SO(RT::RAIDER, 2)})
			}},
			{Factory::factorygunship, array<SQueue> = {
				SQueue(0.5f, {SO(RT::BUILDER), SO(RT::SKIRM, 2), SO(RT::SUPPORT), SO(RT::SKIRM, 2)}),
				SQueue(0.5f, {SO(RT::SCOUT), SO(RT::SUPPORT), SO(RT::BUILDER), SO(RT::SUPPORT), SO(RT::SKIRM, 2), SO(RT::SUPPORT), SO(RT::SKIRM, 2)})
			}},
			{Factory::factoryamph, array<SQueue> = {
				SQueue(0.1f, {SO(RT::BUILDER), SO(RT::RIOT), SO(RT::BUILDER), SO(RT::RAIDER, 5)}),
				SQueue(0.9f, {SO(RT::RAIDER, 5), SO(RT::BUILDER, 2), SO(RT::RIOT)})
			}},
			{Factory::factoryveh, array<SQueue> = {
				SQueue(0.1f, {SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::SCOUT), SO(RT::SKIRM), SO(RT::BUILDER), SO(RT::RAIDER, 6)}),
				SQueue(0.9f, {SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::SCOUT), SO(RT::RIOT), SO(RT::BUILDER), SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::RAIDER, 3)})
			}},
			{Factory::factoryhover, array<SQueue> = {
				SQueue(0.9f, {SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::RAIDER, 3), SO(RT::BUILDER), SO(RT::RAIDER, 5), SO(RT::BUILDER)}),
				SQueue(0.1f, {SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::RAIDER, 3), SO(RT::BUILDER), SO(RT::SKIRM), SO(RT::BUILDER), SO(RT::SKIRM, 2)})
			}},
			{Factory::factoryplane, array<SQueue> = {
				SQueue(0.5f, {SO(RT::AA), SO(RT::BUILDER), SO(RT::AA, 2)}),
				SQueue(0.5f, {SO(RT::AA), SO(RT::BUILDER), SO(RT::AA), SO(RT::BUILDER)})
			}},
			{Factory::factorytank, array<SQueue> = {
				SQueue(0.5f, {SO(RT::BUILDER), SO(RT::SCOUT, 2), SO(RT::BUILDER), SO(RT::RAIDER, 3)}),
				SQueue(0.5f, {SO(RT::BUILDER), SO(RT::SCOUT, 2), SO(RT::BUILDER), SO(RT::RIOT)})
			}},
			{Factory::factoryspider, array<SQueue> = {
				SQueue(0.5f, {SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::SCOUT, 5), SO(RT::RIOT), SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::SCOUT), SO(RT::RIOT), SO(RT::SCOUT), SO(RT::BUILDER)}),
				SQueue(0.5f, {SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::SCOUT, 4), SO(RT::RIOT), SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::SUPPORT)})
			}},
			{Factory::factoryshield, array<SQueue> = {
				SQueue(0.5f, {SO(RT::BUILDER), SO(RT::SCOUT), SO(RT::RAIDER), SO(RT::SCOUT, 2), SO(RT::RAIDER, 2), SO(RT::BUILDER, 2)}),
				SQueue(0.5f, {SO(RT::BUILDER), SO(RT::SCOUT,2), SO(RT::RAIDER, 2), SO(RT::BUILDER), SO(RT::RAIDER, 3)})
			}},
			{Factory::factoryjump, array<SQueue> = {
				SQueue(0.5f, {SO(RT::SCOUT, 2), SO(RT::RAIDER), SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::SCOUT), SO(RT::RAIDER)}),
				SQueue(0.5f, {SO(RT::SCOUT, 4), SO(RT::BUILDER), SO(RT::RAIDER), SO(RT::SCOUT), SO(RT::RAIDER)})
			}}
			}, {SO(RT::RIOT), SO(RT::RAIDER), SO(RT::BUILDER)})
		},
		{Commander::recon, SOpener({
			{Factory::factorycloak, array<SQueue> = {
				SQueue(0.9f, {SO(RT::RAIDER, 2), SO(RT::BUILDER), SO(RT::RAIDER, 4), SO(RT::BUILDER), SO(RT::RAIDER, 4)}),
				SQueue(0.1f, {SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::RIOT), SO(RT::BUILDER), SO(RT::RAIDER, 4)})
			}},
			{Factory::factorygunship, array<SQueue> = {
				SQueue(0.8f, {SO(RT::SUPPORT, 4), SO(RT::SKIRM, 2), SO(RT::SUPPORT), SO(RT::SKIRM, 2)}),
				SQueue(0.2f, {SO(RT::SCOUT), SO(RT::SUPPORT), SO(RT::BUILDER), SO(RT::SUPPORT), SO(RT::SKIRM, 2), SO(RT::SUPPORT), SO(RT::SKIRM, 2)})
			}},
			{Factory::factoryamph, array<SQueue> = {
				SQueue(0.1f, {SO(RT::BUILDER), SO(RT::RIOT), SO(RT::BUILDER), SO(RT::RAIDER, 5)}),
				SQueue(0.9f, {SO(RT::RAIDER, 5), SO(RT::BUILDER, 2), SO(RT::RIOT)})
			}},
			{Factory::factoryveh, array<SQueue> = {
				SQueue(0.2f, {SO(RT::SCOUT, 2), SO(RT::BUILDER), SO(RT::RAIDER, 4), SO(RT::BUILDER), SO(RT::SCOUT)}),
				SQueue(0.8f, {SO(RT::SCOUT),  SO(RT::RAIDER), SO(RT::SCOUT), SO(RT::RAIDER, 3), SO(RT::BUILDER, 2), SO(RT::SCOUT), SO(RT::RAIDER, 2)})
			}},
			{Factory::factoryhover, array<SQueue> = {
				SQueue(0.5f, {SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::RAIDER, 4), SO(RT::BUILDER), SO(RT::RAIDER, 5), SO(RT::BUILDER)}),
				SQueue(0.5f, {SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::RAIDER, 4), SO(RT::BUILDER), SO(RT::SKIRM), SO(RT::BUILDER), SO(RT::SKIRM, 2)})
			}},
			{Factory::factoryplane, array<SQueue> = {
				SQueue(0.5f, {SO(RT::AA), SO(RT::BUILDER), SO(RT::AA, 2)}),
				SQueue(0.5f, {SO(RT::AA), SO(RT::BUILDER), SO(RT::AA), SO(RT::BUILDER)})
			}},
			{Factory::factorytank, array<SQueue> = {
				SQueue(0.5f, {SO(RT::BUILDER), SO(RT::SCOUT, 2), SO(RT::BUILDER), SO(RT::RAIDER, 3)}),
				SQueue(0.5f, {SO(RT::BUILDER), SO(RT::SCOUT, 2), SO(RT::BUILDER), SO(RT::ASSAULT)})
			}},
			{Factory::factoryspider, array<SQueue> = {
				SQueue(0.5f, {SO(RT::SCOUT, 6), SO(RT::RIOT), SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::SCOUT), SO(RT::RIOT), SO(RT::SCOUT), SO(RT::BUILDER)}),
				SQueue(0.5f, {SO(RT::SCOUT, 12), SO(RT::BUILDER), SO(RT::RIOT), SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::SUPPORT)})
			}},
			{Factory::factoryshield, array<SQueue> = {
				SQueue(0.5f, {SO(RT::SCOUT), SO(RT::RAIDER, 3), SO(RT::BUILDER, 2)}),
				SQueue(0.5f, {SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::RAIDER, 2), SO(RT::BUILDER), SO(RT::RAIDER, 3)})
			}},
			{Factory::factoryjump, array<SQueue> = {
				SQueue(0.5f, {SO(RT::SCOUT, 2), SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::SCOUT), SO(RT::RAIDER)}),
				SQueue(0.5f, {SO(RT::SCOUT, 3), SO(RT::BUILDER), SO(RT::SCOUT), SO(RT::RAIDER), SO(RT::SCOUT), SO(RT::RAIDER)})
			}}
			}, {SO(RT::RAIDER, 2), SO(RT::BUILDER)})
		},
		{Commander::assault, SOpener({
			{Factory::factorycloak, array<SQueue> = {
				SQueue(0.8f, {SO(RT::RAIDER, 2), SO(RT::BUILDER), SO(RT::RAIDER, 2), SO(RT::BUILDER), SO(RT::RAIDER, 4)}),
				SQueue(0.2f, {SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::RIOT), SO(RT::BUILDER), SO(RT::RAIDER, 4)})
			}},
			{Factory::factorygunship, array<SQueue> = {
				SQueue(0.5f, {SO(RT::SUPPORT, 4), SO(RT::SKIRM, 2), SO(RT::SUPPORT), SO(RT::SKIRM, 2)}),
				SQueue(0.5f, {SO(RT::AH)})
			}},
			{Factory::factoryamph, array<SQueue> = {
				SQueue(0.2f, {SO(RT::BUILDER), SO(RT::RIOT), SO(RT::BUILDER), SO(RT::RAIDER, 5)}),
				SQueue(0.8f, {SO(RT::RAIDER, 5), SO(RT::BUILDER, 2), SO(RT::RIOT), SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::RAIDER)})
			}},
			{Factory::factoryveh, array<SQueue> = {
				SQueue(0.10f, {SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::SKIRM), SO(RT::BUILDER), SO(RT::RIOT), SO(RT::BUILDER), SO(RT::SCOUT, 3), SO(RT::SKIRM, 3)}),
				SQueue(0.15f, {SO(RT::SCOUT, 2), SO(RT::BUILDER), SO(RT::RIOT), SO(RT::BUILDER), SO(RT::SKIRM), SO(RT::BUILDER), SO(RT::SKIRM, 4), SO(RT::BUILDER), SO(RT::RIOT)}),
				SQueue(0.75f, {SO(RT::SCOUT, 2), SO(RT::RAIDER, 2), SO(RT::BUILDER, 2), SO(RT::RAIDER, 5), SO(RT::BUILDER), SO(RT::SCOUT)})
			}},
			{Factory::factoryhover, array<SQueue> = {
				SQueue(0.5f, {SO(RT::BUILDER), SO(RT::RAIDER, 4), SO(RT::BUILDER), SO(RT::RAIDER, 5), SO(RT::BUILDER)}),
				SQueue(0.5f, {SO(RT::BUILDER), SO(RT::RAIDER, 4), SO(RT::BUILDER), SO(RT::SKIRM), SO(RT::BUILDER), SO(RT::SKIRM, 2)})
			}},
			{Factory::factoryplane, array<SQueue> = {
				SQueue(0.5f, {SO(RT::AA), SO(RT::BUILDER), SO(RT::AA, 2)}),
				SQueue(0.5f, {SO(RT::AA), SO(RT::BUILDER), SO(RT::AA), SO(RT::BUILDER)})
			}},
			{Factory::factorytank, array<SQueue> = {
				SQueue(0.50f, {SO(RT::RIOT), SO(RT::BUILDER), SO(RT::ASSAULT), SO(RT::BUILDER), SO(RT::ASSAULT), SO(RT::BUILDER, 2), SO(RT::HEAVY)}),
				SQueue(0.25f, {SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::RAIDER, 3), SO(RT::BUILDER), SO(RT::ASSAULT)}),
				SQueue(0.25f, {SO(RT::BUILDER), SO(RT::RIOT), SO(RT::BUILDER), SO(RT::RIOT), SO(RT::ASSAULT), SO(RT::BUILDER), SO(RT::ASSAULT)})
			}},
			{Factory::factoryspider, array<SQueue> = {
				SQueue(0.5f, {SO(RT::SCOUT, 3), SO(RT::BUILDER), SO(RT::SCOUT, 3), SO(RT::RIOT), SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::SCOUT), SO(RT::RIOT), SO(RT::SCOUT), SO(RT::BUILDER)}),
				SQueue(0.5f, {SO(RT::SCOUT, 12), SO(RT::BUILDER), SO(RT::RIOT), SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::SUPPORT)})
			}},
			{Factory::factoryshield, array<SQueue> = {
				SQueue(0.5f, {SO(RT::SCOUT, 3), SO(RT::BUILDER), SO(RT::SCOUT), SO(RT::RAIDER, 3), SO(RT::BUILDER, 2)}),
				SQueue(0.5f, {SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::RAIDER, 2), SO(RT::SCOUT, 3), SO(RT::BUILDER), SO(RT::RAIDER, 3)})
			}},
			{Factory::factoryjump, array<SQueue> = {
				SQueue(0.5f, {SO(RT::SCOUT, 2), SO(RT::RAIDER), SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::ASSAULT)}),
				SQueue(0.5f, {SO(RT::SCOUT, 2), SO(RT::BUILDER), SO(RT::RAIDER), SO(RT::SCOUT), SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::RAIDER)})
			}}
			}, {SO(RT::RAIDER, 2), SO(RT::BUILDER)})
		},
		{Commander::strike, SOpener({
			{Factory::factorycloak, array<SQueue> = {
				SQueue(0.8f, {SO(RT::RAIDER, 2), SO(RT::BUILDER), SO(RT::RAIDER, 2), SO(RT::BUILDER), SO(RT::RAIDER, 4)}),
				SQueue(0.2f, {SO(RT::BUILDER), SO(RT::RIOT), SO(RT::BUILDER), SO(RT::RAIDER, 4)})
			}},
			{Factory::factorygunship, array<SQueue> = {
				SQueue(0.5f, {SO(RT::SUPPORT, 4), SO(RT::SKIRM, 2), SO(RT::SUPPORT), SO(RT::SKIRM, 2)}),
				SQueue(0.5f, {SO(RT::SCOUT), SO(RT::SUPPORT), SO(RT::BUILDER), SO(RT::SUPPORT), SO(RT::SKIRM, 2), SO(RT::SUPPORT), SO(RT::SKIRM, 2)})
			}},
			{Factory::factoryamph, array<SQueue> = {
				SQueue(0.2f, {SO(RT::BUILDER), SO(RT::RIOT), SO(RT::BUILDER), SO(RT::RAIDER, 5)}),
				SQueue(0.8f, {SO(RT::RAIDER, 5), SO(RT::BUILDER, 2), SO(RT::RIOT), SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::RAIDER)})
			}},
			{Factory::factoryveh, array<SQueue> = {
				SQueue(0.1f, {SO(RT::SCOUT, 3), SO(RT::BUILDER), SO(RT::SKIRM), SO(RT::BUILDER), SO(RT::SKIRM, 3), SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::RIOT)}),
				SQueue(0.9f, {SO(RT::SCOUT, 2), SO(RT::RAIDER, 5), SO(RT::BUILDER, 2), SO(RT::SCOUT), SO(RT::RAIDER, 5), SO(RT::BUILDER)})
			}},
			{Factory::factoryhover, array<SQueue> = {
				SQueue(0.5f, {SO(RT::BUILDER), SO(RT::RAIDER, 4), SO(RT::BUILDER), SO(RT::RAIDER, 5), SO(RT::BUILDER)}),
				SQueue(0.5f, {SO(RT::BUILDER), SO(RT::RAIDER, 4), SO(RT::BUILDER), SO(RT::RAIDER, 3), SO(RT::SKIRM), SO(RT::BUILDER), SO(RT::SKIRM, 2)})
			}},
			{Factory::factoryplane, array<SQueue> = {
				SQueue(0.5f, {SO(RT::AA), SO(RT::BUILDER), SO(RT::AA, 2)}),
				SQueue(0.5f, {SO(RT::AA), SO(RT::BUILDER), SO(RT::AA), SO(RT::BUILDER)})
			}},
			{Factory::factorytank, array<SQueue> = {
				SQueue(0.25f, {SO(RT::RIOT), SO(RT::BUILDER), SO(RT::ASSAULT), SO(RT::BUILDER), SO(RT::ASSAULT), SO(RT::BUILDER, 2), SO(RT::ASSAULT)}),
				SQueue(0.50f, {SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::RAIDER), SO(RT::BUILDER), SO(RT::RAIDER, 3), SO(RT::BUILDER), SO(RT::ASSAULT)}),
				SQueue(0.25f, {SO(RT::BUILDER), SO(RT::RIOT), SO(RT::BUILDER), SO(RT::RIOT), SO(RT::ASSAULT), SO(RT::BUILDER), SO(RT::ASSAULT)})
			}},
			{Factory::factoryspider, array<SQueue> = {
				SQueue(0.5f, {SO(RT::SCOUT, 6), SO(RT::RIOT), SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::SCOUT), SO(RT::RIOT), SO(RT::SCOUT), SO(RT::BUILDER)}),
				SQueue(0.5f, {SO(RT::SCOUT, 12), SO(RT::BUILDER), SO(RT::RIOT), SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::SUPPORT)})
			}},
			{Factory::factoryshield, array<SQueue> = {
				SQueue(0.5f, {SO(RT::SCOUT), SO(RT::RAIDER, 3), SO(RT::BUILDER, 2)}),
				SQueue(0.5f, {SO(RT::SCOUT), SO(RT::BUILDER), SO(RT::RAIDER, 2), SO(RT::BUILDER), SO(RT::RAIDER, 3)})
			}},
			{Factory::factoryjump, array<SQueue> = {
				SQueue(0.5f, {SO(RT::SCOUT, 4), SO(RT::RAIDER, 2)}),
				SQueue(0.5f, {SO(RT::SCOUT, 4), SO(RT::BUILDER), SO(RT::RAIDER, 2)})
			}}
			}, {SO(RT::RAIDER, 2), SO(RT::BUILDER)})
		}
	};
}

const array<SO>@ GetOpener(const CCircuitDef@ facDef)
{
	dictionary@ openInfo = GetOpenInfo();
	const CCircuitDef@ commChoice = aiSetupMgr.GetCommChoice();
	const string commName = commChoice.GetName();

	SOpener@ open;  // null
	array<string>@ keys = openInfo.getKeys();
	for (uint i = 0, l = keys.length(); i < l; ++i) {
		if (commName.findFirst(keys[i]) >= 0) {
			@open = cast<SOpener>(openInfo[keys[i]]);
			break;
		}
	}

	if (open is null) {
		return null;
	}

	const string facName = facDef.GetName();
	array<SQueue>@ queues;
	if (open.factory.get(facName, @queues)) {
		array<float> weights;
		for (uint i = 0, l = queues.length(); i < l; ++i) {
			weights.insertLast(queues[i].weight);
		}
		int choice = aiDice(weights);
		if (choice < 0) {
			return open.def;
		}
		return queues[choice].orders;
	}
	return open.def;
}

}


namespace Hide {

// Commander hides if ("frame" elapsed) and ("threat" exceeds value or enemy has "air")
shared class SHide {
	SHide(int f, float t, bool a) {
		frame = f;
		threat = t;
		isAir = a;
	}
	int frame;
	float threat;
	bool isAir;
}

dictionary hideInfo = {
	{Commander::support, SHide(480 * 30, 30.f, true)},
	{Commander::recon, SHide(470 * 30, 20.f, true)},
	{Commander::assault, SHide(460 * 30, 60.f, true)},
	{Commander::strike, SHide(450 * 30, 50.f, true)}
};

map<Id, SHide@> hideUnitDef;  // cache map<UnitDef_Id, SHide>

const SHide@ CacheHide(const CCircuitDef@ cdef)
{
	Id cid = cdef.GetId();
	const string name = cdef.GetName();
	array<string>@ keys = hideInfo.getKeys();
	for (uint i = 0, l = keys.length(); i < l; ++i) {
		if (name.findFirst(keys[i]) >= 0) {
			SHide@ hide = cast<SHide>(hideInfo[keys[i]]);
			hideUnitDef.insert(cid, hide);
			return hide;
		}
	}
	hideUnitDef.insert(cid, null);
	return null;
}


const SHide@ GetForUnitDef(const CCircuitDef@ cdef)
{
	bool success;
	SHide@ hide = hideUnitDef.find(cdef.GetId(), success);
	return success ? hide : CacheHide(cdef);
}

}
