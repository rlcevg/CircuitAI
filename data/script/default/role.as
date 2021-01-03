namespace Role {

TypeMask BUILDER = aiRoleMasker.GetTypeMask("builder");
TypeMask SCOUT   = aiRoleMasker.GetTypeMask("scout");
TypeMask RAIDER  = aiRoleMasker.GetTypeMask("raider");
TypeMask RIOT    = aiRoleMasker.GetTypeMask("riot");
TypeMask ASSAULT = aiRoleMasker.GetTypeMask("assault");
TypeMask SKIRM   = aiRoleMasker.GetTypeMask("skirmish");
TypeMask ARTY    = aiRoleMasker.GetTypeMask("artillery");
TypeMask AA      = aiRoleMasker.GetTypeMask("anti_air");
TypeMask AS      = aiRoleMasker.GetTypeMask("anti_sub");
TypeMask AH      = aiRoleMasker.GetTypeMask("anti_heavy");
TypeMask BOMBER  = aiRoleMasker.GetTypeMask("bomber");
TypeMask SUPPORT = aiRoleMasker.GetTypeMask("support");
TypeMask MINE    = aiRoleMasker.GetTypeMask("mine");
TypeMask TRANS   = aiRoleMasker.GetTypeMask("transport");
TypeMask AIR     = aiRoleMasker.GetTypeMask("air");
TypeMask SUB     = aiRoleMasker.GetTypeMask("sub");
TypeMask STATIC  = aiRoleMasker.GetTypeMask("static");
TypeMask HEAVY   = aiRoleMasker.GetTypeMask("heavy");
TypeMask SUPER   = aiRoleMasker.GetTypeMask("super");
TypeMask COMM    = aiRoleMasker.GetTypeMask("commander");

TypeMask ROLE0   = AiAddRole("cloaked_raider",  RT::ASSAULT);
TypeMask ROLE1   = AiAddRole("snipe_target",    RT::ASSAULT);
TypeMask ROLE2   = AiAddRole("bullshit_raider", RT::ASSAULT);
TypeMask ROLE3   = AiAddRole("disarm_target",   RT::ASSAULT);
TypeMask ROLE4   = AiAddRole("shieldball",      RT::ASSAULT);
TypeMask ROLE5   = AiAddRole("missileskirm",    RT::ASSAULT);
TypeMask ROLE6   = AiAddRole("turtle",          RT::ASSAULT);
TypeMask ROLE7   = AiAddRole("role7",           RT::ASSAULT);
TypeMask ROLE8   = AiAddRole("role8",           RT::ASSAULT);
TypeMask ROLE9   = AiAddRole("role9",           RT::ASSAULT);
TypeMask ROLE10  = AiAddRole("role10",          RT::ASSAULT);
TypeMask ROLE11  = AiAddRole("builderT2",       RT::BUILDER);

}

namespace RT {

Type BUILDER = Role::BUILDER.type;
Type SCOUT   = Role::SCOUT.type;
Type RAIDER  = Role::RAIDER.type;
Type RIOT    = Role::RIOT.type;
Type ASSAULT = Role::ASSAULT.type;
Type SKIRM   = Role::SKIRM.type;
Type ARTY    = Role::ARTY.type;
Type AA      = Role::AA.type;
Type AS      = Role::AS.type;
Type AH      = Role::AH.type;
Type BOMBER  = Role::BOMBER.type;
Type SUPPORT = Role::SUPPORT.type;
Type MINE    = Role::MINE.type;
Type TRANS   = Role::TRANS.type;
Type AIR     = Role::AIR.type;
Type SUB     = Role::SUB.type;
Type STATIC  = Role::STATIC.type;
Type HEAVY   = Role::HEAVY.type;
Type SUPER   = Role::SUPER.type;
Type COMM    = Role::COMM.type;

}

namespace RM {

Mask BUILDER = Role::BUILDER.mask;
Mask SCOUT   = Role::SCOUT.mask;
Mask RAIDER  = Role::RAIDER.mask;
Mask RIOT    = Role::RIOT.mask;
Mask ASSAULT = Role::ASSAULT.mask;
Mask SKIRM   = Role::SKIRM.mask;
Mask ARTY    = Role::ARTY.mask;
Mask AA      = Role::AA.mask;
Mask AS      = Role::AS.mask;
Mask AH      = Role::AH.mask;
Mask BOMBER  = Role::BOMBER.mask;
Mask SUPPORT = Role::SUPPORT.mask;
Mask MINE    = Role::MINE.mask;
Mask TRANS   = Role::TRANS.mask;
Mask AIR     = Role::AIR.mask;
Mask SUB     = Role::SUB.mask;
Mask STATIC  = Role::STATIC.mask;
Mask HEAVY   = Role::HEAVY.mask;
Mask SUPER   = Role::SUPER.mask;
Mask COMM    = Role::COMM.mask;

}
