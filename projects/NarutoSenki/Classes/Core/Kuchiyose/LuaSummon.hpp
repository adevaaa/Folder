#pragma once
#include <cmath>
#include "Hero.hpp"
#include "Systems/SummonConfig.hpp"

/**
 * LuaSummon
 *
 * AI generik untuk unit yang dideklarasikan di AiCompat.lua (#Kugutsu / #Kuchiyose / #Ninja)
 * tetapi tidak punya kelas AI sendiri, mis. "Rhino", "Puppet", "MotoiOwl".
 *
 * Perilaku:
 *   - cari musuh sesuai target / untarget dan Area dari definisi #Clone
 *   - ada musuh   -> dekati lalu nattack (skill otomatis ditangani SummonSystem::tick lewat Attack = ...)
 *   - tidak ada   -> ikuti owner selama Area bukan Free
 */
class LuaSummon : public Hero
{
	void perform() override
	{
		const SummonDef *def = _luaDef;
		const bool free = def && def->isFree();
		const bool useMaster = _master && !free;
		const int range = def ? (free ? 288 : def->radius() + 32) : 256;

		_mainTarget = nullptr;
		if (!findHero(range, useMaster))
			if (!findFlog(range, useMaster))
				findTower(range, useMaster);

		if (_mainTarget)
		{
			Vec2 sp = getDistanceToTargetAndIgnoreOriginY();
			const float reachX = _nAttackRangeX > 32 ? _nAttackRangeX : 32;
			if (std::abs(sp.x) > reachX || std::abs(sp.y) > 24)
			{
				if (isFreeState())
					walk(sp.getNormalized());
			}
			else if (isFreeState())
			{
				changeSide(sp);
				attack(NAttack);
			}
			return;
		}

		if (useMaster)
		{
			const float follow = def && def->hasArea() ? (def->radius() / 2 > 24 ? def->radius() / 2 : 24) : 64;
			if (std::abs((_master->getPosition() - getPosition()).x) > follow)
			{
				if (isFreeState())
					walk(getDirByMoveTo(_master));
				return;
			}
		}

		if (_state == State::WALK || _state == State::NATTACK)
			idle();
	}
};
