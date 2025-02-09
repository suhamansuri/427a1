// internal
#include "physics_system.hpp"
#include "world_init.hpp"
#include <iostream>

// Returns the local bounding coordinates scaled by the current size of the entity
vec2 get_bounding_box(const Motion& motion)
{
	// abs is to avoid negative scale due to the facing direction.
	return { abs(motion.scale.x), abs(motion.scale.y) };
}

// This is a SUPER APPROXIMATE check that puts a circle around the bounding boxes and sees
// if the center point of either object is inside the other's bounding-box-circle. You can
// surely implement a more accurate detection
bool collides(const Motion& motion1, const Motion& motion2)
{
	// vec2 dp = motion1.position - motion2.position;
	// float dist_squared = dot(dp,dp);
	// const vec2 other_bonding_box = get_bounding_box(motion1) / 2.f;
	// const float other_r_squared = dot(other_bonding_box, other_bonding_box);
	// const vec2 my_bonding_box = get_bounding_box(motion2) / 2.f;
	// const float my_r_squared = dot(my_bonding_box, my_bonding_box);
	// const float r_squared = max(other_r_squared, my_r_squared);
	// if (dist_squared < r_squared)
	// 	return true;
	// return false;
	vec2 pos1 = motion1.position;
	vec2 pos2 = motion2.position;
	vec2 half_motion1 = get_bounding_box(motion1) / 2.0f;
	vec2 half_motion2 = get_bounding_box(motion2) / 2.0f;

	float left1 = pos1.x - half_motion1.x;
	float right1 = pos1.x + half_motion1.x;
	float up1 = pos1.y - half_motion1.y;
	float down1 = pos1.y + half_motion1.y;

	float left2 = pos2.x - half_motion2.x;
	float right2 = pos2.x + half_motion2.x;
	float up2 = pos2.y - half_motion2.y;
	float down2 = pos2.y + half_motion2.y;

	bool h_overlap = left1 < right2 && right1 > left2;
	bool v_overlap = up1 < down2 && down1 > up2;

	return h_overlap && v_overlap;

}

void PhysicsSystem::step(float elapsed_ms)
{
	// Move each entity that has motion (invaders, projectiles, and even towers [they have 0 for velocity])
	// based on how much time has passed, this is to (partially) avoid
	// having entities move at different speed based on the machine.
	auto& motion_registry = registry.motions;
	for(uint i = 0; i< motion_registry.size(); i++)
	{
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// !!! TODO A1: update motion.position based on step_seconds and motion.velocity
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		Motion& motion = motion_registry.components[i];
		Entity entity = motion_registry.entities[i];
		float step_seconds = elapsed_ms / 1000.f;
		
		motion.position += motion.velocity * step_seconds;
	}

	// check for collisions between all moving entities
    ComponentContainer<Motion> &motion_container = registry.motions;
	for(uint i = 0; i < motion_container.components.size(); i++)
	{
		Motion& motion_i = motion_container.components[i];
		Entity entity_i = motion_container.entities[i];
		
		//  starting j at i+1 to compare all pairs only once 
		for(uint j = i+1; j < motion_container.components.size(); j++)
		{
			Motion& motion_j = motion_container.components[j];
			if (collides(motion_i, motion_j))
			{
				Entity entity_j = motion_container.entities[j];
				// Create a collisions event
				// We are abusing the ECS system a bit in that we potentially insert muliple collisions for the same entity
				// CK: why the duplication, except to allow searching by entity_id
				registry.collisions.emplace_with_duplicates(entity_i, entity_j);
				registry.collisions.emplace_with_duplicates(entity_j, entity_i);

				// registry.collisions.emplace_with_duplicates(entity_j, entity_i);
			}
		}
	}
}