#include <iostream>
#include "ai_system.hpp"
#include "world_init.hpp"

void AISystem::step(float elapsed_ms)
{

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// !!! TODO A1: scan for invaders and shoot at them
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// invader detection system for towers
	// - for each tower, scan its row:
	//   - if an invader is detected and the tower's shooting timer has expired,
	//     then shoot (create a projectile) and reset the tower's shot timer
	for (const Entity& tower_entity : registry.towers.entities) {
		Tower& tower = registry.towers.get(tower_entity);
		Motion& tower_motion = registry.motions.get(tower_entity);

		tower.timer_ms -= ((int)elapsed_ms);
		if (tower.timer_ms > 0) continue;

		for (const Entity& invader_entity : registry.invaders.entities) {
			Motion& invader_motion = registry.motions.get(invader_entity);

			if (abs(invader_motion.position.y - tower_motion.position.y) < INVADER_BB_HEIGHT / 2) {
				createProjectile(
					tower_motion.position,
					{GRID_CELL_WIDTH_PX / 6, GRID_CELL_HEIGHT_PX / 6},
					{ -GRID_CELL_WIDTH_PX * 5, 0.f}
				);

				tower.timer_ms = TOWER_TIMER_MS;
				break;
			}
		}
	}
}