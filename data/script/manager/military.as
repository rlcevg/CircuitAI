#include "../role.as"


/*
 * anti-air threat threshold;
 * air factories will stop production when AA threat exceeds
 */
bool isAirValid() {
	return enemyMgr.GetEnemyThreat(RT::AA) <= 80.f;
}
