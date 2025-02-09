// Header
#include "world_system.hpp"
#include "world_init.hpp"

// stlib
#include <cassert>
#include <sstream>
#include <iostream>

#include "physics_system.hpp"

// create the world
WorldSystem::WorldSystem() :
	points(0),
	max_towers(MAX_TOWERS_START),
	next_invader_spawn(0),
	invader_spawn_rate_ms(INVADER_SPAWN_RATE_MS)
{
	// seeding rng with random device
	rng = std::default_random_engine(std::random_device()());
}

WorldSystem::~WorldSystem() {
	// Destroy music components
	if (background_music != nullptr)
		Mix_FreeMusic(background_music);
	if (chicken_dead_sound != nullptr)
		Mix_FreeChunk(chicken_dead_sound);
	if (chicken_eat_sound != nullptr)
		Mix_FreeChunk(chicken_eat_sound);
	Mix_CloseAudio();

	// Destroy all created components
	registry.clear_all_components();

	// Close the window
	glfwDestroyWindow(window);
}

// Debugging
namespace {
	void glfw_err_cb(int error, const char *desc) {
		std::cerr << error << ": " << desc << std::endl;
	}
}

// call to close the window, wrapper around GLFW commands
void WorldSystem::close_window() {
	glfwSetWindowShouldClose(window, GLFW_TRUE);
}

// World initialization
// Note, this has a lot of OpenGL specific things, could be moved to the renderer
GLFWwindow* WorldSystem::create_window() {

	///////////////////////////////////////
	// Initialize GLFW
	glfwSetErrorCallback(glfw_err_cb);
	if (!glfwInit()) {
		std::cerr << "ERROR: Failed to initialize GLFW in world_system.cpp" << std::endl;
		return nullptr;
	}

	//-------------------------------------------------------------------------
	// If you are on Linux or Windows, you can change these 2 numbers to 4 and 3 and
	// enable the glDebugMessageCallback to have OpenGL catch your mistakes for you.
	// GLFW / OGL Initialization
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#if __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	// CK: setting GLFW_SCALE_TO_MONITOR to true will rescale window but then you must handle different scalings
	// glfwWindowHint(GLFW_SCALE_TO_MONITOR, GL_TRUE);		// GLFW 3.3+
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GL_FALSE);		// GLFW 3.3+

	// Create the main window (for rendering, keyboard, and mouse input)
	window = glfwCreateWindow(WINDOW_WIDTH_PX, WINDOW_HEIGHT_PX, "Towers vs Invaders Assignment", nullptr, nullptr);
	if (window == nullptr) {
		std::cerr << "ERROR: Failed to glfwCreateWindow in world_system.cpp" << std::endl;
		return nullptr;
	}

	// Setting callbacks to member functions (that's why the redirect is needed)
	// Input is handled using GLFW, for more info see
	// http://www.glfw.org/docs/latest/input_guide.html
	glfwSetWindowUserPointer(window, this);
	auto key_redirect = [](GLFWwindow* wnd, int _0, int _1, int _2, int _3) { ((WorldSystem*)glfwGetWindowUserPointer(wnd))->on_key(_0, _1, _2, _3); };
	auto cursor_pos_redirect = [](GLFWwindow* wnd, double _0, double _1) { ((WorldSystem*)glfwGetWindowUserPointer(wnd))->on_mouse_move({ _0, _1 }); };
	auto mouse_button_pressed_redirect = [](GLFWwindow* wnd, int _button, int _action, int _mods) { ((WorldSystem*)glfwGetWindowUserPointer(wnd))->on_mouse_button_pressed(_button, _action, _mods); };
	
	glfwSetKeyCallback(window, key_redirect);
	glfwSetCursorPosCallback(window, cursor_pos_redirect);
	glfwSetMouseButtonCallback(window, mouse_button_pressed_redirect);

	return window;
}

bool WorldSystem::start_and_load_sounds() {
	
	//////////////////////////////////////
	// Loading music and sounds with SDL
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "Failed to initialize SDL Audio");
		return false;
	}

	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == -1) {
		fprintf(stderr, "Failed to open audio device");
		return false;
	}

	background_music = Mix_LoadMUS(audio_path("music.wav").c_str());
	chicken_dead_sound = Mix_LoadWAV(audio_path("chicken_dead.wav").c_str());
	chicken_eat_sound = Mix_LoadWAV(audio_path("chicken_eat.wav").c_str());

	if (background_music == nullptr || chicken_dead_sound == nullptr || chicken_eat_sound == nullptr) {
		fprintf(stderr, "Failed to load sounds\n %s\n %s\n %s\n make sure the data directory is present",
			audio_path("music.wav").c_str(),
			audio_path("chicken_dead.wav").c_str(),
			audio_path("chicken_eat.wav").c_str());
		return false;
	}

	return true;
}

void WorldSystem::init(RenderSystem* renderer_arg) {

	this->renderer = renderer_arg;

	// start playing background music indefinitely
	std::cout << "Starting music..." << std::endl;
	Mix_PlayMusic(background_music, -1);

	// Set all states to default
    restart_game();
}

// Update our game world
bool WorldSystem::step(float elapsed_ms_since_last_update) {

	// Updating window title with points
	std::stringstream title_ss;
	title_ss << "Points: " << points;
	glfwSetWindowTitle(window, title_ss.str().c_str());

	// Remove debug info from the last step
	while (registry.debugComponents.entities.size() > 0)
	    registry.remove_all_components_of(registry.debugComponents.entities.back());

	// Removing out of screen entities
	auto& motions_registry = registry.motions;
	//  entities that leave the screen on the right side
	for (int i = (int)motions_registry.components.size()-1; i>=0; --i) {
	    Motion& motion = motions_registry.components[i];
		Entity entity = motions_registry.entities[i];

		//  game over if an invader exits the right side
		if (registry.invaders.has(entity) && (motion.position.x - (motion.scale.x / 2)) > WINDOW_WIDTH_PX) {
			game_over = true; 
		}
		if (motion.position.x + abs(motion.scale.x) < 0.f) {
			if(!registry.players.has(motions_registry.entities[i])) // don't remove the player
				registry.remove_all_components_of(motions_registry.entities[i]);
		}
		ScreenState& screen = registry.screenStates.components[0];
		if (!game_over) { 
			if (screen.darken_screen_factor > 0) {
				screen.darken_screen_factor -= elapsed_ms_since_last_update / 5000.0f;
				if (screen.darken_screen_factor < 0) {
					screen.darken_screen_factor = 0;
				}
			}
		}

	}

	// walking animation
	for (Entity entity : registry.invaders.entities) {
		Invader invader = registry.invaders.get(entity);
		if (registry.animations.has(entity)) {
			Animation& anim = registry.animations.get(entity);
			anim.timer += elapsed_ms_since_last_update / 1000.0f;

			if (anim.timer >= anim.frame_duration) {
				anim.timer = 0.0f;
    	        anim.current_frame = (anim.current_frame + 1) % anim.total_frames; 
				RenderRequest& render_request = registry.renderRequests.get(entity);
				if (invader.type == 0) {
					if (anim.current_frame == 0) {
						render_request.used_texture = TEXTURE_ASSET_ID::INVADER_IDLE_BLUE;
					} else if (anim.current_frame == 1) {
						render_request.used_texture = TEXTURE_ASSET_ID::INVADER_WALKING_BLUE;
					} else {
						render_request.used_texture = TEXTURE_ASSET_ID::INVADER_RUNNING_BLUE;
					}
				} else if (invader.type == 1) {
					if (anim.current_frame == 0) {
						render_request.used_texture = TEXTURE_ASSET_ID::INVADER_GREEN_ONE;
					} else if (anim.current_frame == 1) {
						render_request.used_texture = TEXTURE_ASSET_ID::INVADER_GREEN_TWO;
					} else {
						render_request.used_texture = TEXTURE_ASSET_ID::INVADER_GREEN_THREE;
					}
				}
			}
		}
	}

	// spawn new invaders
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// TODO A1: limit them to cells on the far-left, except (0, 0)
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	next_invader_spawn -= ((int)(elapsed_ms_since_last_update* current_speed));
	if (next_invader_spawn < 0.f) {
		if (!game_over) {
			// reset timer
			next_invader_spawn = ((int)((INVADER_SPAWN_RATE_MS / 2) + uniform_dist(rng) * (INVADER_SPAWN_RATE_MS / 2)));

			int total_rows = WINDOW_HEIGHT_PX / GRID_CELL_HEIGHT_PX;
			int row = 1 + (rand() % (total_rows - 1));
			int col = 0;

			// create invader with random initial position
			Entity invader = createInvader(renderer, vec2(
				col * GRID_CELL_WIDTH_PX + GRID_CELL_WIDTH_PX / 2,
				row * GRID_CELL_HEIGHT_PX + GRID_CELL_HEIGHT_PX / 2
			),
			(rand() % 2));
			Animation& animation = registry.animations.emplace(invader);
			animation.frame_duration = 0.1f;
			animation.is_walking = true;
			animation.current_frame = 0;
			animation.total_frames = 3; 
		}
	}
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// TODO A1: game over fade out
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	assert(registry.screenStates.components.size() <= 1);
    ScreenState &screen = registry.screenStates.components[0];

	if (game_over) {
		ScreenState& screen = registry.screenStates.components[0];

		screen.darken_screen_factor += elapsed_ms_since_last_update / 1500.0f;
		//std::cout << screen.darken_screen_factor << std::endl;

		if (screen.darken_screen_factor >= 1.0f) {
			screen.darken_screen_factor = 1.0f;
		}

		for (auto& motion : registry.motions.components) {
			motion.velocity = {0.f, 0.f};
		}

		return false;  
	}

	if (screen.apply_vignette) {
		//std::cout << screen.apply_vignette << std::endl;
		static float vignette_duration = 1000.0f; 
		vignette_duration -= elapsed_ms_since_last_update;

		if (vignette_duration <= 0.0f) {
			screen.apply_vignette = 0;   
			vignette_duration = 1000.0f; 
		}
	}

    float min_counter_ms = 3000.f;
	for (Entity entity : registry.deathTimers.entities) {

		// progress timer
		DeathTimer& counter = registry.deathTimers.get(entity);
		counter.counter_ms -= elapsed_ms_since_last_update;
		if(counter.counter_ms < min_counter_ms){
		    min_counter_ms = counter.counter_ms;
		}

		/* for A1, let the user press "R" to restart instead
		// restart the game once the death timer expires
		if (counter.counter_ms < 0) {
			registry.deathTimers.remove(entity);
			screen.darken_screen_factor = 0;
            restart_game();
			return true;
		}
		*/
	}

	// reduce window brightness if any of the present chickens is dying
	screen.darken_screen_factor = 1 - min_counter_ms / 3000;
	if (screen.darken_screen_factor >= 1.0f) {
		screen.darken_screen_factor = 1.0f;
		registry.deathTimers.clear();  
	}


	// particle explosion stepping for collisions between tower and invader
	for (Entity entity: registry.explosions.entities) {
		Explosion& explosion = registry.explosions.get(entity);

		explosion.timer += elapsed_ms_since_last_update / 1000.0f;

		// update all paticle elements
		for (auto& particle : explosion.particles) {
			particle.lifespan -= elapsed_ms_since_last_update / 1000.0f;
			particle.position += particle.velocity * (elapsed_ms_since_last_update / 1000.0f);
			particle.color.a = particle.lifespan / 1.0f; // reduce intensity
		}

		// remove particles that are passed their lifespan
		explosion.particles.erase(
			std::remove_if(explosion.particles.begin(), explosion.particles.end(),
						[](const Particle& p) { return p.lifespan <= 0; }),
			explosion.particles.end()
		);

		// if no particles, explosion should be removed
		if (explosion.particles.empty() || explosion.timer >= explosion.duration) {
			registry.remove_all_components_of(entity);
		}
	}



	return true;
}

// Reset the world state to its initial state
void WorldSystem::restart_game() {

	std::cout << "Restarting..." << std::endl;

	// Debugging for memory/component leaks
	registry.list_all_components();

	// Reset the game speed
	current_speed = 1.f;

	points = 0;
	max_towers = MAX_TOWERS_START;
	next_invader_spawn = 0;
	invader_spawn_rate_ms = INVADER_SPAWN_RATE_MS;
	game_over = false;

	// Remove all entities that we created
	// All that have a motion, we could also iterate over all bug, eagles, ... but that would be more cumbersome
	while (registry.motions.entities.size() > 0)
	    registry.remove_all_components_of(registry.motions.entities.back());

	// debugging for memory/component leaks
	registry.list_all_components();

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// TODO A1: create grid lines
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	int grid_line_width = GRID_LINE_WIDTH_PX;

	// create grid lines if they do not already exist
	if (grid_lines.size() == 0) {
		// vertical lines
		int cell_width = GRID_CELL_WIDTH_PX;
		for (int col = 0; col < 14 + 1; col++) {
			// width of 2 to make the grid easier to see
			grid_lines.push_back(createGridLine(vec2(col * cell_width, 0), vec2(grid_line_width, 2 * WINDOW_HEIGHT_PX)));
		}

		// horizontal lines
		int cell_height = GRID_CELL_HEIGHT_PX;
		for (int col = 0; col < 10 + 1; col++) {
			// width of 2 to make the grid easier to see
			grid_lines.push_back(createGridLine(vec2(0, col * cell_height), vec2(2 * WINDOW_WIDTH_PX, grid_line_width)));
		}
	}
}

// Compute collisions between entities
void WorldSystem::handle_collisions() {

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// TODO A1: Loop over all collisions detected by the physics system
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	ComponentContainer<Collision>& collision_container = registry.collisions;
	for (uint i = 0; i < collision_container.components.size(); i++) {
		
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// TODO A1: handle collision between deadly (projectile) and invader
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// Mix_PlayChannel(-1, chicken_dead_sound, 0);

		Entity invader = collision_container.entities[i];
		Entity other = collision_container.components[i].other;

		if (registry.invaders.has(invader) && registry.projectiles.has(other)) {
			Invader& inv = registry.invaders.get(invader);
			Motion& invader_motion = registry.motions.get(invader);
			inv.health -= PROJECTILE_DAMAGE;

			registry.remove_all_components_of(other);

			if (inv.health <= 0) {
				createExplosion(invader_motion.position, vec4(0.8f, 0.1f, 1.0f, 1.0f), 20); 

				registry.remove_all_components_of(invader);
				Mix_PlayChannel(-1, chicken_dead_sound, 0);
				
				points++;
			}
		}


		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// TODO A1: handle collision between tower and invader
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// Mix_PlayChannel(-1, chicken_eat_sound, 0);

		if (registry.invaders.has(invader) && registry.towers.has(other)) {
			Motion& tower_motion = registry.motions.get(other);
			createExplosion(tower_motion.position, vec4(0.0f, 1.0f, 0.4f, 1.0f), 20);

			registry.remove_all_components_of(invader);
			registry.remove_all_components_of(other);
			Mix_PlayChannel(-1, chicken_eat_sound, 0);

			if (max_towers > 0) {
                max_towers--;
                std::cout << "Tower destroyed! Max towers reduced to: " << max_towers << std::endl;
			}

			ScreenState& screen = registry.screenStates.components[0];
			screen.apply_vignette = 1;  
			screen.vignette_intensity = 1.0f; 
		}

	}

	// Remove all collisions from this simulation step
	registry.collisions.clear();
}

// Should the game be over ?
bool WorldSystem::is_over() const {
	return bool(glfwWindowShouldClose(window));
}

// on key callback
void WorldSystem::on_key(int key, int, int action, int mod) {

	// exit game w/ ESC
	if (action == GLFW_RELEASE && key == GLFW_KEY_ESCAPE) {
		close_window();
	}

	// Resetting game
	if (action == GLFW_RELEASE && key == GLFW_KEY_R) {
		int w, h;
		glfwGetWindowSize(window, &w, &h);

        restart_game();
	}

	// Debugging - not used in A1, but left intact for the debug lines
	if (key == GLFW_KEY_D) {
		if (action == GLFW_RELEASE) {
			if (debugging.in_debug_mode) {
				debugging.in_debug_mode = false;
			}
			else {
				debugging.in_debug_mode = true;
			}
		}
	}
}

void WorldSystem::on_mouse_move(vec2 mouse_position) {

	// record the current mouse position
	mouse_pos_x = mouse_position.x;
	mouse_pos_y = mouse_position.y;
}

void WorldSystem::on_mouse_button_pressed(int button, int action, int mods) {

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// TODO A1: Handle mouse clicking for invader and tower placement.
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	// on button press
	if (action == GLFW_PRESS) {

		int tile_x = (int)(mouse_pos_x / GRID_CELL_WIDTH_PX);
		int tile_y = (int)(mouse_pos_y / GRID_CELL_HEIGHT_PX);

		std::cout << "mouse position: " << mouse_pos_x << ", " << mouse_pos_y << std::endl;
		std::cout << "mouse tile position: " << tile_x << ", " << tile_y << std::endl;

		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// TODO A1: place invaders on the left, except top left spot
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

		if (tile_x == 0 && tile_y > 0) {
			Entity invader = createInvader(renderer, vec2(tile_x * GRID_CELL_WIDTH_PX + GRID_CELL_WIDTH_PX / 2, tile_y * GRID_CELL_HEIGHT_PX + GRID_CELL_HEIGHT_PX / 2), 
			(rand() % 2));
			// Clicking will only be random as well
			Animation& animation = registry.animations.emplace(invader);
			animation.frame_duration = 0.3f;
			animation.is_walking = true;
			animation.current_frame = 0;
			animation.total_frames = 3; 
		}



		if (tile_x == 13) {
			if (button == GLFW_MOUSE_BUTTON_LEFT) {
            	if ((int)registry.towers.entities.size() < max_towers) {
					removeTower(vec2(
						tile_x * GRID_CELL_WIDTH_PX + GRID_CELL_WIDTH_PX / 2,
						tile_y * GRID_CELL_HEIGHT_PX + GRID_CELL_HEIGHT_PX / 2
					));
					createTower(renderer, vec2(
						tile_x * GRID_CELL_WIDTH_PX + GRID_CELL_WIDTH_PX / 2,
						tile_y * GRID_CELL_HEIGHT_PX + GRID_CELL_HEIGHT_PX / 2
					));
				} else {
					std::cout << "Maximum number of towers (" << max_towers << ") reached!" << std::endl;
				}
			} else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
				removeTower(vec2(
					tile_x * GRID_CELL_WIDTH_PX + GRID_CELL_WIDTH_PX / 2,
					tile_y * GRID_CELL_HEIGHT_PX + GRID_CELL_HEIGHT_PX / 2
				));

			}
		}
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// TODO A1: place a tower on the right, except top right spot
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		
			// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			// TODO A1: right-click removes towers
			// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			
			// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			// TODO A1: left-click adds new tower (removing any existing towers), up to max_towers
			// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	}
}


// function to create explosion after collision. using particles
void WorldSystem::createExplosion(const vec2& position, const vec4& color, int num_particles = 20) {
	Entity explosion_entity = Entity();
	Explosion& explosion = registry.explosions.emplace(explosion_entity);

	Explosion exp;
	exp.duration = 1.0f;
	exp.timer = 0.0f;

	// initialize particles in the explosion
	for (int i = 0; i < num_particles; ++i) {
		Particle particle;
		float angle = ((float)rand() / RAND_MAX) * 2 * M_PI;
		float speed = ((float)rand() / RAND_MAX) * 100.0f + 50.0f;

		particle.velocity = {cos(angle)*speed, sin(angle)* speed};
		particle.lifespan = 1.0f;
		particle.rotation_speed = ((float)rand() / RAND_MAX) * 4.0f - 2.0f;
		particle.color = color;
		particle.position = position;
		explosion.particles.push_back(particle);

	}

}