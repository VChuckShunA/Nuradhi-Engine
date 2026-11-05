#include "game/level_editor.hpp"
#include "game/game_components.hpp"
#include "engine/core/components.hpp"
#include "engine/effects/blood.hpp"
#include "engine/effects/lightning.hpp"
#include "engine/fog/fog_volume.hpp"
#include "engine/particles/particles.hpp"
#include "engine/physics/collision.hpp"
#include "engine/rain/nuradhi_rain.hpp"
#include "engine/rain/rain_volume.hpp"
#include "engine/snow/snow_volume.hpp"
#include "engine/sprite/sprite.hpp"

//libs
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

//std
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <memory>
#include <string>
#include <vector>

namespace game {

	using nrd::ecs::Entity;

	namespace {
		constexpr float FLOOR_TOP = 1.5f; //y of the test level's floor surface (+y is down)

		struct Prefab {
			const char* name;
			const char* meshPath;
			glm::vec3 scale;
			EditorBounds bounds; //of the mesh, before scaling
			float spawnY; //puts the prop on the floor
			nrd::MeshHandle mesh{};
		};

		struct EditorState {
			bool open = false;
			Entity selected = nrd::ecs::NullEntity;
			int prefab = 0;
			int placedCount = 0;
			ImGuizmo::OPERATION gizmo = ImGuizmo::TRANSLATE;
			ImGuizmo::MODE gizmoSpace = ImGuizmo::WORLD;
			nrd::TextureHandle propTexture{};
			std::vector<Prefab> prefabs;
			EditorActions actions;
			std::string actionMessage; //what the last button press did
		};

		nrd::Collider colliderFromBounds(const EditorBounds& bounds, const glm::vec3& scale)
		{
			const glm::vec3 halfExtents = glm::abs((bounds.max - bounds.min) * 0.5f * scale);
			const glm::vec3 offset = (bounds.max + bounds.min) * 0.5f * scale;
			return nrd::Collider::box(halfExtents, offset);
		}

		//where new props appear: on the floor, under the camera's focus
		glm::vec3 placementPoint(nrd::ecs::Registry& registry, float spawnY)
		{
			glm::vec3 point{ 0.f, spawnY, 3.f };
			const Entity camera = registry.findFirst<nrd::Transform, nrd::Camera>();
			if (camera == nrd::ecs::NullEntity) return point;
			if (const nrd::CameraFollow* follow = registry.tryGet<nrd::CameraFollow>(camera)) {
				if (const nrd::Transform* target = registry.tryGet<nrd::Transform>(follow->target)) {
					point.x = target->position.x + 0.6f; //a little in front of the player
					point.z = target->position.z;
					return point;
				}
			}
			point.x = registry.get<nrd::Transform>(camera).position.x;
			return point;
		}

		Entity place(nrd::ecs::Registry& registry, EditorState& state, const Prefab& prefab)
		{
			const Entity entity = registry.create();
			registry.add<nrd::Transform>(entity, nrd::Transform{ placementPoint(registry, prefab.spawnY), glm::vec3{ 0.f }, prefab.scale });
			registry.add<nrd::MeshRenderer>(entity, nrd::MeshRenderer{ prefab.mesh, state.propTexture });
			registry.add<nrd::Collider>(entity, colliderFromBounds(prefab.bounds, prefab.scale));
			registry.add<EditorBounds>(entity, prefab.bounds);
			registry.add<nrd::Name>(entity, nrd::Name{ std::string{ prefab.name } + " " + std::to_string(++state.placedCount) });
			return entity;
		}

		//keeps the hitbox matched to the mesh after a scale change
		void rescaleCollider(nrd::ecs::Registry& registry, Entity entity, const glm::vec3& oldScale, const glm::vec3& newScale)
		{
			nrd::Collider* collider = registry.tryGet<nrd::Collider>(entity);
			if (!collider) return;
			if (const EditorBounds* bounds = registry.tryGet<EditorBounds>(entity)) {
				const bool isStatic = collider->isStatic;
				*collider = colliderFromBounds(*bounds, newScale);
				collider->isStatic = isStatic;
				return;
			}
			for (int axis = 0; axis < 3; axis++) {
				if (oldScale[axis] == 0.f) continue;
				const float ratio = newScale[axis] / oldScale[axis];
				collider->halfExtents[axis] *= std::abs(ratio);
				collider->offset[axis] *= ratio;
			}
		}

		//Transform::mat4 builds translate * Ry * Rx * Rz * scale; this takes such a matrix apart again
		void decompose(const glm::mat4& matrix, glm::vec3& position, glm::vec3& rotation, glm::vec3& scale)
		{
			position = glm::vec3(matrix[3]);
			glm::vec3 axes[3] = { glm::vec3(matrix[0]), glm::vec3(matrix[1]), glm::vec3(matrix[2]) };
			for (int i = 0; i < 3; i++) {
				scale[i] = glm::length(axes[i]);
				if (scale[i] > 0.f) axes[i] /= scale[i];
			}
			//column 2 is (cos x sin y, -sin x, cos x cos y); row 1 of columns 0 and 1 is (cos x sin z, cos x cos z)
			rotation.x = std::asin(glm::clamp(-axes[2].y, -1.f, 1.f));
			rotation.y = std::atan2(axes[2].x, axes[2].z);
			rotation.z = std::atan2(axes[0].y, axes[1].y);
		}

		//the move/rotate/scale handles on the selected entity, drawn over the scene
		void gizmo(nrd::Engine& engine, EditorState& state)
		{
			nrd::ecs::Registry& registry = engine.registry();
			ImGuizmo::BeginFrame();
			if (state.selected == nrd::ecs::NullEntity) return;

			glm::mat4 view{ 1.f }, projection{ 1.f };
			engine.renderer().cameraMatrices(registry, view, projection);
			//our clip space has +y down (Vulkan); ImGuizmo expects OpenGL's +y up, so flip it
			const glm::mat4 gizmoProjection = glm::scale(glm::mat4{ 1.f }, glm::vec3{ 1.f, -1.f, 1.f }) * projection;

			const Entity camera = registry.findFirst<nrd::Transform, nrd::Camera>();
			const bool orthographic = camera == nrd::ecs::NullEntity || registry.get<nrd::Camera>(camera).projection == nrd::Camera::Projection::Orthographic;
			const ImGuiIO& io = ImGui::GetIO();
			ImGuizmo::SetOrthographic(orthographic);
			ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList()); //over the scene, under the editor window
			ImGuizmo::SetRect(0.f, 0.f, io.DisplaySize.x, io.DisplaySize.y);

			//sprites always face the camera at their own size, so they only get the move arrows
			const ImGuizmo::OPERATION operation = registry.has<nrd::Sprite>(state.selected) ? ImGuizmo::TRANSLATE : state.gizmo;
			nrd::Transform& transform = registry.get<nrd::Transform>(state.selected);
			glm::mat4 matrix = transform.mat4();
			if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(gizmoProjection), operation, state.gizmoSpace, glm::value_ptr(matrix))) {
				glm::vec3 position, rotation, scale;
				decompose(matrix, position, rotation, scale);
				//copy back only what this handle changes, so the other values don't pick up rounding drift
				if (operation == ImGuizmo::TRANSLATE) {
					transform.position = position;
				}
				else if (operation == ImGuizmo::ROTATE) {
					transform.rotation = rotation;
				}
				else {
					scale = glm::max(scale, glm::vec3{ 0.01f });
					rescaleCollider(registry, state.selected, transform.scale, scale);
					transform.scale = scale;
				}
			}
			//a click on a handle is for the editor, not for Blaze's fists
			if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) engine.input().consumeMouse();
		}

		void gizmoControls(nrd::Engine& engine, EditorState& state)
		{
			//1/2/3 switch handles (W/E/R would fight with WASD)
			const nrd::Input& input = engine.input();
			if (input.wasPressed(nrd::Key::Num1)) state.gizmo = ImGuizmo::TRANSLATE;
			if (input.wasPressed(nrd::Key::Num2)) state.gizmo = ImGuizmo::ROTATE;
			if (input.wasPressed(nrd::Key::Num3)) state.gizmo = ImGuizmo::SCALE;

			ImGui::SeparatorText("Gizmo");
			if (ImGui::RadioButton("Move (1)", state.gizmo == ImGuizmo::TRANSLATE)) state.gizmo = ImGuizmo::TRANSLATE;
			ImGui::SameLine();
			if (ImGui::RadioButton("Rotate (2)", state.gizmo == ImGuizmo::ROTATE)) state.gizmo = ImGuizmo::ROTATE;
			ImGui::SameLine();
			if (ImGui::RadioButton("Scale (3)", state.gizmo == ImGuizmo::SCALE)) state.gizmo = ImGuizmo::SCALE;
			if (ImGui::RadioButton("World", state.gizmoSpace == ImGuizmo::WORLD)) state.gizmoSpace = ImGuizmo::WORLD;
			ImGui::SameLine();
			if (ImGui::RadioButton("Local", state.gizmoSpace == ImGuizmo::LOCAL)) state.gizmoSpace = ImGuizmo::LOCAL;
			ImGui::TextDisabled("+y points down in this engine, so the green arrow does too.");
		}

		void selectedObjectPanel(nrd::Engine& engine, EditorState& state)
		{
			nrd::ecs::Registry& registry = engine.registry();
			nrd::Transform& transform = registry.get<nrd::Transform>(state.selected);
			const nrd::Name* name = registry.tryGet<nrd::Name>(state.selected);
			ImGui::SeparatorText(name ? name->value.c_str() : "Selected");

			ImGui::DragFloat3("Position", &transform.position.x, 0.01f);

			glm::vec3 degrees = glm::degrees(transform.rotation);
			if (ImGui::DragFloat3("Rotation", &degrees.x, 0.5f)) transform.rotation = glm::radians(degrees);

			glm::vec3 scale = transform.scale;
			if (ImGui::DragFloat3("Scale", &scale.x, 0.01f, 0.01f, 100.f)) {
				scale = glm::max(scale, glm::vec3{ 0.01f });
				rescaleCollider(registry, state.selected, transform.scale, scale);
				transform.scale = scale;
			}

			if (registry.has<nrd::Sprite>(state.selected)) {
				ImGui::TextDisabled("Sprites ignore rotation and scale.");
			}
			else if (registry.has<nrd::Collider>(state.selected)) {
				ImGui::TextDisabled("The hitbox follows scale, not rotation.");
			}

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.6f, 0.15f, 0.15f, 1.f });
			if (ImGui::Button("Remove")) {
				registry.destroy(state.selected);
				state.selected = nrd::ecs::NullEntity;
			}
			ImGui::PopStyleColor();
		}

		void editorWindow(nrd::Engine& engine, EditorState& state)
		{
			nrd::ecs::Registry& registry = engine.registry();
			ImGui::SetNextWindowPos(ImVec2{ 20.f, 20.f }, ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2{ 380.f, 560.f }, ImGuiCond_FirstUseEver);
			if (!ImGui::Begin("Level Editor", &state.open)) {
				ImGui::End();
				return;
			}

			ImGui::Checkbox("Pause game", &engine.paused);
			bool debugLines = engine.renderer().debugOverlay();
			if (ImGui::Checkbox("Debug lines (F1)", &debugLines)) engine.renderer().setDebugOverlay(debugLines);
			ImGui::SetItemTooltip("Wireframes, collider boxes and attack hitboxes");
			ImGui::TextDisabled("F2 closes the editor");

			ImGui::SeparatorText("Filters");
			{
				nrd::PostProcessSettings& filters = engine.renderer().postProcess();
				ImGui::Checkbox("Film noir (F3)", &filters.noir.enabled);
				if (ImGui::TreeNode("Film noir settings")) {
					if (ImGui::Button("Reset##noir")) filters.noir = nrd::FilmNoirFilter{ filters.noir.enabled };
					ImGui::SliderFloat("contrast", &filters.noir.contrast, 0.5f, 3.f, "%.2f");
					ImGui::SliderFloat("brightness", &filters.noir.brightness, -0.5f, 0.5f, "%.2f");
					ImGui::SliderFloat("grain", &filters.noir.grain, 0.f, 0.3f, "%.3f");
					ImGui::SliderFloat("vignette", &filters.noir.vignette, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("keep red", &filters.noir.keepRed, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("keep blue", &filters.noir.keepBlue, 0.f, 1.f, "%.2f");
					ImGui::TreePop();
				}
				ImGui::Checkbox("CRT (F4)", &filters.crt.enabled);
				if (ImGui::TreeNode("CRT settings")) {
					if (ImGui::Button("Reset##crt")) filters.crt = nrd::CrtFilter{ filters.crt.enabled };
					ImGui::SliderFloat("curvature", &filters.crt.curvature, 0.f, 0.3f, "%.3f");
					ImGui::SliderFloat("scanlines", &filters.crt.scanlines, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("scanline size", &filters.crt.scanlineSize, 1.f, 10.f, "%.1f px");
					ImGui::SliderFloat("phosphor mask", &filters.crt.mask, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("colour fringing", &filters.crt.aberration, 0.f, 6.f, "%.1f px");
					ImGui::SliderFloat("flicker", &filters.crt.flicker, 0.f, 0.2f, "%.3f");
					ImGui::SliderFloat("edge darkening", &filters.crt.vignette, 0.f, 1.f, "%.2f");
					ImGui::TreePop();
				}
			}

			if (state.actions.rebakeSprites) {
				ImGui::SeparatorText("Assets");
				if (ImGui::Button("Rebake sprites")) {
					try {
						state.actionMessage = state.actions.rebakeSprites();
					}
					catch (const std::exception& error) {
						state.actionMessage = std::string("Rebake failed, the old sprites are still in use: ") + error.what();
					}
				}
				ImGui::SetItemTooltip("Builds Summers' atlas again from her PNGs and loads it into the game now.\nThe game stands still for a few seconds while it works.");
				if (!state.actionMessage.empty()) ImGui::TextWrapped("%s", state.actionMessage.c_str());
			}

			ImGui::SeparatorText("Enemies");
			registry.each<EnemySquad>([&](Entity, EnemySquad& squad) {
				ImGui::SliderInt("attack at once", &squad.maxAttackers, 1, 5);
				ImGui::SliderFloat("gap between attackers", &squad.attackGap, 0.f, 3.f, "%.2f s");
				ImGui::SliderFloat("standoff", &squad.standoff, 0.6f, 3.f, "%.2f");
				ImGui::SliderFloat("waiter spacing", &squad.slotSpacing, 0.2f, 1.2f, "%.2f");
				ImGui::SliderFloat("min rest", &squad.minCooldown, 0.f, 4.f, "%.2f s");
				ImGui::SliderFloat("max rest", &squad.maxCooldown, 0.f, 6.f, "%.2f s");
				squad.maxCooldown = std::max(squad.maxCooldown, squad.minCooldown);
			});

			ImGui::SeparatorText("Blood");
			registry.each<nrd::BloodEffect>([&](Entity entity, nrd::BloodEffect& blood) {
				ImGui::PushID(static_cast<int>(entity));
				if (ImGui::Button("Spray")) {
					//a test spray from the first character's chest, on to the floor under them
					const Entity target = registry.findFirst<nrd::Sprite, nrd::Transform>();
					if (target != nrd::ecs::NullEntity) {
						const glm::vec3 feet = registry.get<nrd::Transform>(target).position;
						nrd::spawnBlood(registry, feet - glm::vec3{ 0.f, 0.6f, 0.f }, { 1.f, -0.6f, 0.f }, 1.f, feet.y);
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("Clean up")) {
					blood.droplets.clear();
					blood.splatsOnGround.clear();
				}
				if (ImGui::TreeNode("Blood settings")) {
					if (ImGui::Button("Reset")) blood = nrd::BloodEffect{};
					ImGui::ColorEdit3("colour", &blood.colour.x);
					ImGui::SliderInt("droplets per hit", &blood.dropletsPerHit, 1, 120);
					ImGui::SliderFloat("spray speed", &blood.spraySpeed, 0.2f, 8.f, "%.2f");
					ImGui::SliderFloat("spread", &blood.spread, 0.f, 1.5f, "%.2f");
					ImGui::SliderFloat("droplet size", &blood.dropletSize, 0.002f, 0.05f, "%.3f");
					ImGui::SliderFloat("gravity", &blood.gravity, 0.f, 30.f, "%.1f");
					ImGui::Checkbox("splats", &blood.splats);
					ImGui::SliderFloat("splat chance", &blood.splatChance, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("splat size", &blood.splatSize, 0.005f, 0.2f, "%.3f");
					ImGui::SliderFloat("splat life", &blood.splatLife, 0.f, 300.f, "%.0f s");
					ImGui::SliderFloat("rain wash", &blood.rainWash, 0.f, 0.5f, "%.2f per s");
					ImGui::SliderFloat("splat fade", &blood.splatFade, 0.f, 10.f, "%.1f s");
					ImGui::TreePop();
				}
				ImGui::PopID();
			});

			ImGui::SeparatorText("Weather");
			registry.each<nrd::Lightning, nrd::Name>([&](Entity entity, nrd::Lightning& lightning, nrd::Name& name) {
				ImGui::PushID(static_cast<int>(entity));
				ImGui::Checkbox(name.value.c_str(), &lightning.enabled);
				ImGui::SameLine();
				if (ImGui::Button("Strike now")) nrd::strikeLightning(registry, entity);
				if (ImGui::TreeNode("Lightning settings")) {
					if (ImGui::Button("Reset")) lightning = nrd::Lightning{ lightning.enabled };
					ImGui::SliderFloat("average wait", &lightning.meanInterval, 1.f, 60.f, "%.0f s");
					ImGui::Checkbox("only while it rains", &lightning.onlyInRain);
					ImGui::SeparatorText("Flash");
					ImGui::SliderFloat("strength", &lightning.flashStrength, 0.f, 2.f, "%.2f");
					ImGui::SliderFloat("scene light", &lightning.sceneLight, 0.f, 10.f, "%.1f");
					ImGui::SliderFloat("sky glow", &lightning.skyGlow, 0.f, 1.f, "%.2f");
					ImGui::ColorEdit3("colour", &lightning.colour.x);
					ImGui::SeparatorText("Bolt");
					ImGui::Checkbox("show the bolt", &lightning.showBolt);
					ImGui::SliderFloat("spread", &lightning.halfWidth, 0.5f, 6.f, "%.2f");
					ImGui::SliderFloat("height", &lightning.height, 1.f, 8.f, "%.2f");
					ImGui::SliderFloat("depth", &lightning.depth, 0.f, 6.f, "%.2f");
					ImGui::SliderFloat("thickness", &lightning.boltWidth, 0.003f, 0.06f, "%.3f");
					ImGui::SliderFloat("glow", &lightning.glow, 1.f, 12.f, "%.1f");
					ImGui::SliderFloat("forks", &lightning.branches, 0.f, 1.f, "%.2f");
					ImGui::TreePop();
				}
				ImGui::PopID();
			});
			registry.each<nrd::ParticleEmitter, nrd::Name>([&](Entity entity, nrd::ParticleEmitter& emitter, nrd::Name& name) {
				if (registry.tryGet<BulletTracers>(entity)) return; //draws the uzi bullets; not weather
				ImGui::PushID(static_cast<int>(entity));
				ImGui::Checkbox(name.value.c_str(), &emitter.enabled);
				ImGui::SameLine(120.f);
				ImGui::SetNextItemWidth(150.f);
				ImGui::SliderFloat("density", &emitter.density, 0.f, 2.f, "%.2f");
				ImGui::PopID();
			});
			registry.each<nrd::FogVolume, nrd::Name>([&](Entity entity, nrd::FogVolume& fog, nrd::Name& name) {
				ImGui::PushID(static_cast<int>(entity));
				ImGui::Checkbox(name.value.c_str(), &fog.enabled);
				if (ImGui::TreeNode("Fog settings")) {
					if (ImGui::Button("Cinematic")) fog = nrd::FogVolume::cinematic();
					ImGui::SameLine();
					if (ImGui::Button("Graphic novel")) fog = nrd::FogVolume::graphicNovel();
					ImGui::SameLine();
					if (ImGui::Button("Realistic")) fog = nrd::FogVolume::realistic();
					ImGui::SliderFloat("height", &fog.height, 0.05f, 1.5f, "%.2f");
					ImGui::SliderInt("slices", &fog.slices, 4, 96);
					ImGui::ColorEdit3("colour", &fog.colour.x);
					ImGui::SliderFloat("density", &fog.density, 0.f, 30.f, "%.1f");
					ImGui::SliderFloat("coverage", &fog.coverage, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("height falloff", &fog.heightFalloff, 0.f, 8.f, "%.2f");
					ImGui::SliderFloat("noise scale", &fog.noiseScale, 0.2f, 6.f, "%.2f");
					ImGui::SliderFloat("turbulence", &fog.turbulence, 0.f, 2.f, "%.2f");
					ImGui::SliderFloat2("wind", &fog.wind.x, -1.f, 1.f, "%.2f");
					ImGui::SliderFloat("shadow", &fog.shadow, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("light glow", &fog.lightScatter, 0.f, 30.f, "%.1f");
					ImGui::SeparatorText("Graphic novel");
					ImGui::SliderInt("tone bands", &fog.toneBands, 0, 8);
					ImGui::SliderFloat("halftone", &fog.halftone, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("dot spacing", &fog.halftoneSize, 2.f, 20.f, "%.1f px");
					ImGui::SeparatorText("Interaction");
					ImGui::Checkbox("characters push the fog", &fog.interactive);
					ImGui::BeginDisabled(!fog.interactive);
					ImGui::SliderFloat("body push", &fog.bodyPush, 0.f, 3.f, "%.2f");
					ImGui::SliderFloat("limb push", &fog.limbPush, 0.f, 3.f, "%.2f");
					ImGui::SliderFloat("landing puff", &fog.landingPuff, 0.f, 3.f, "%.2f");
					ImGui::SliderFloat("refill time", &fog.refillTime, 0.2f, 10.f, "%.1f s");
					ImGui::SliderFloat("swirl", &fog.swirl, 0.f, 2.f, "%.2f");
					ImGui::SliderFloat("body thickness", &fog.bodyThickness, 0.05f, 1.f, "%.2f");
					ImGui::EndDisabled();
					ImGui::TreePop();
				}
				ImGui::PopID();
			});
			registry.each<nrd::NuradhiRain, nrd::Name>([&](Entity entity, nrd::NuradhiRain& rain, nrd::Name& name) {
				ImGui::PushID(static_cast<int>(entity));
				if (ImGui::TreeNode("rain", "%s settings", name.value.c_str())) {
					if (ImGui::Button("Reset")) rain = nrd::NuradhiRain{};
					ImGui::SeparatorText("Droplets");
					ImGui::SliderFloat("speed", &rain.fallSpeed, 0.5f, 25.f, "%.1f");
					ImGui::SliderFloat("size", &rain.dropSize, 0.001f, 0.03f, "%.4f");
					ImGui::SliderFloat("length", &rain.dropLength, 0.005f, 0.8f, "%.3f");
					ImGui::SliderFloat("wind speed", &rain.windSpeed, 0.f, 8.f, "%.2f");
					ImGui::SliderAngle("wind direction", &rain.windDirection, -180.f, 180.f);
					ImGui::SeparatorText("Ripples");
					ImGui::Checkbox("ripples", &rain.ripples);
					ImGui::SliderFloat("ripple chance", &rain.rippleChance, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("ripple size", &rain.rippleSize, 0.01f, 0.4f, "%.3f");
					ImGui::SliderFloat("ripple life", &rain.rippleLife, 0.1f, 3.f, "%.2f s");
					ImGui::SliderFloat("ripple strength", &rain.rippleStrength, 0.f, 1.f, "%.2f");
					ImGui::SeparatorText("Splashes");
					ImGui::Checkbox("splashes", &rain.splashes);
					ImGui::SliderFloat("splash chance", &rain.splashChance, 0.f, 1.f, "%.2f");
					ImGui::SliderInt("droplets", &rain.splashDroplets, 1, 12);
					ImGui::SliderFloat("splash speed", &rain.splashSpeed, 0.1f, 3.f, "%.2f");
					ImGui::SliderFloat("droplet size", &rain.splashSize, 0.001f, 0.02f, "%.4f");
					ImGui::SliderFloat("splash strength", &rain.splashStrength, 0.f, 1.f, "%.2f");
					ImGui::SeparatorText("Look");
					ImGui::ColorEdit3("water colour", &rain.colour.x);
					ImGui::SliderFloat("light glint", &rain.lightGlint, 0.f, 30.f, "%.1f");
					ImGui::TreePop();
				}
				ImGui::PopID();
			});
			registry.each<nrd::RainVolume, nrd::Name>([&](Entity entity, nrd::RainVolume& rain, nrd::Name& name) {
				ImGui::PushID(static_cast<int>(entity));
				ImGui::Checkbox(name.value.c_str(), &rain.enabled);
				if (ImGui::TreeNode("Rain settings")) {
					if (ImGui::Button("Cinematic")) rain = nrd::RainVolume::cinematic();
					ImGui::SameLine();
					if (ImGui::Button("Realistic")) rain = nrd::RainVolume::realisticRain();
					ImGui::SeparatorText("Droplets");
					ImGui::SliderFloat("speed", &rain.fallSpeed, 0.5f, 25.f, "%.1f");
					ImGui::SliderFloat("size", &rain.dropSize, 0.0003f, 0.01f, "%.4f");
					ImGui::SliderFloat("length", &rain.dropLength, 0.005f, 0.6f, "%.3f");
					ImGui::SliderFloat("spacing", &rain.spacing, 0.01f, 0.3f, "%.3f");
					ImGui::SliderFloat("density", &rain.density, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("depth scaling", &rain.depthScaling, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("tail fade", &rain.tailFade, 0.f, 1.f, "%.2f");
					ImGui::SeparatorText("Wind");
					ImGui::SliderFloat("wind speed", &rain.windSpeed, 0.f, 8.f, "%.2f");
					ImGui::SliderAngle("wind direction", &rain.windDirection, -180.f, 180.f);
					ImGui::SliderFloat("gusts", &rain.gusts, 0.f, 1.f, "%.2f");
					ImGui::SeparatorText("Look");
					ImGui::SliderInt("layers", &rain.layers, 1, 32);
					ImGui::SliderFloat("depth", &rain.depth, 0.f, 4.f, "%.2f");
					ImGui::ColorEdit4("colour", &rain.colour.x);
					ImGui::SliderFloat("depth of field", &rain.depthOfField, 0.f, 0.03f, "%.4f");
					ImGui::SliderFloat("focus", &rain.focus, -4.f, 4.f, "%.2f");
					ImGui::SliderFloat("mist", &rain.mist, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("light glint", &rain.lightScatter, 0.f, 30.f, "%.1f");
					ImGui::SeparatorText("Realistic");
					ImGui::Checkbox("splashes and ripples", &rain.realistic);
					ImGui::BeginDisabled(!rain.realistic);
					ImGui::SliderFloat("splashes", &rain.splashes, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("ripple rate", &rain.rippleRate, 0.f, 6.f, "%.2f");
					ImGui::SliderFloat("ripple size", &rain.rippleSize, 0.01f, 0.4f, "%.3f");
					ImGui::SliderFloat("ripple life", &rain.rippleLife, 0.1f, 3.f, "%.2f s");
					ImGui::SliderFloat("ripple strength", &rain.rippleStrength, 0.f, 1.f, "%.2f");
					ImGui::EndDisabled();
					ImGui::TreePop();
				}
				ImGui::PopID();
			});
			registry.each<nrd::SnowVolume, nrd::Name>([&](Entity entity, nrd::SnowVolume& snow, nrd::Name& name) {
				ImGui::PushID(static_cast<int>(entity));
				ImGui::Checkbox(name.value.c_str(), &snow.enabled);
				if (ImGui::TreeNode("Snow settings")) {
					if (ImGui::Button("Cinematic")) snow = nrd::SnowVolume::cinematic();
					ImGui::SameLine();
					if (ImGui::Button("Graphic novel")) snow = nrd::SnowVolume::graphicNovel();
					ImGui::SliderInt("layers", &snow.layers, 1, 32);
					ImGui::SliderFloat("depth", &snow.depth, 0.f, 4.f, "%.2f");
					ImGui::SliderFloat("flake size", &snow.flakeSize, 0.002f, 0.05f, "%.3f");
					ImGui::SliderFloat("spacing", &snow.spacing, 0.03f, 0.5f, "%.2f");
					ImGui::SliderFloat("density", &snow.density, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("fall speed", &snow.fallSpeed, 0.f, 3.f, "%.2f");
					ImGui::SliderFloat("wind", &snow.wind, -2.f, 2.f, "%.2f");
					ImGui::SliderFloat2("sway (amp, Hz)", &snow.sway.x, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("motion blur", &snow.motionBlur, 0.f, 2.f, "%.2f");
					ImGui::ColorEdit4("colour", &snow.colour.x);
					ImGui::SliderFloat("depth of field", &snow.depthOfField, 0.f, 0.1f, "%.3f");
					ImGui::SliderFloat("focus", &snow.focus, -4.f, 4.f, "%.2f");
					ImGui::SliderFloat("mist", &snow.mist, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("light glow", &snow.lightScatter, 0.f, 30.f, "%.1f");
					ImGui::SeparatorText("Graphic novel");
					ImGui::SliderFloat("ink outline", &snow.outline, 0.f, 1.f, "%.2f");
					ImGui::SliderInt("tone bands", &snow.toneBands, 0, 8);
					ImGui::SliderFloat("halftone", &snow.halftone, 0.f, 1.f, "%.2f");
					ImGui::SliderFloat("dot spacing", &snow.halftoneSize, 2.f, 20.f, "%.1f px");
					ImGui::TreePop();
				}
				ImGui::PopID();
			});

			ImGui::SeparatorText("Place");
			if (ImGui::BeginCombo("Prefab", state.prefabs[state.prefab].name)) {
				for (int i = 0; i < static_cast<int>(state.prefabs.size()); i++) {
					if (ImGui::Selectable(state.prefabs[i].name, i == state.prefab)) state.prefab = i;
				}
				ImGui::EndCombo();
			}
			if (ImGui::Button("Place in front of the camera")) state.selected = place(registry, state, state.prefabs[state.prefab]);

			ImGui::SeparatorText("Objects");
			if (ImGui::BeginListBox("##objects", ImVec2{ -FLT_MIN, 180.f })) {
				registry.each<nrd::Name, nrd::Transform>([&](Entity entity, nrd::Name& name, nrd::Transform&) {
					const std::string label = name.value + "##" + std::to_string(entity);
					if (ImGui::Selectable(label.c_str(), entity == state.selected)) state.selected = entity;
				});
				ImGui::EndListBox();
			}

			if (!registry.valid(state.selected) || !registry.has<nrd::Transform>(state.selected)) state.selected = nrd::ecs::NullEntity;
			gizmoControls(engine, state);
			if (state.selected != nrd::ecs::NullEntity) {
				selectedObjectPanel(engine, state);
			}
			else {
				ImGui::TextDisabled("Select an object to edit it.");
			}
			ImGui::End();
			gizmo(engine, state);

			//outline the selection, whether or not the debug lines are on
			if (state.selected != nrd::ecs::NullEntity) {
				const nrd::Transform& transform = registry.get<nrd::Transform>(state.selected);
				if (const nrd::Collider* collider = registry.tryGet<nrd::Collider>(state.selected)) {
					nrd::Aabb box = collider->worldBounds(transform.position);
					box.min -= glm::vec3{ 0.02f };
					box.max += glm::vec3{ 0.02f };
					engine.renderer().drawSelectionBox(box);
				}
				else {
					engine.renderer().drawSelectionBox(nrd::Aabb::fromCenterHalfExtents(transform.position, glm::vec3{ 0.08f }));
				}
			}
		}
	}

	void addLevelEditor(nrd::Engine& engine, nrd::TextureHandle propTexture, EditorActions actions)
	{
		auto state = std::make_shared<EditorState>();
		state->propTexture = propTexture;
		state->actions = std::move(actions);
		const EditorBounds vaseBounds{ { -0.1f, -0.4f, -0.1f }, { 0.1f, 0.f, 0.1f } }; //origin at the base
		const EditorBounds cubeBounds{ glm::vec3{ -1.f }, glm::vec3{ 1.f } };
		const EditorBounds slabBounds{ { -1.f, 0.f, -1.f }, { 1.f, 0.1f, 1.f } }; //top surface at the origin, 0.1 thick
		state->prefabs = {
			{ "Vase", "models/flat_vase.obj", glm::vec3{ 1.f }, vaseBounds, FLOOR_TOP },
			{ "Smooth vase", "models/smooth_vase.obj", glm::vec3{ 1.f }, vaseBounds, FLOOR_TOP },
			{ "Crate", "models/cube.obj", glm::vec3{ 0.25f }, cubeBounds, FLOOR_TOP - 0.25f },
			{ "Coloured crate", "models/colored_cube.obj", glm::vec3{ 0.25f }, cubeBounds, FLOOR_TOP - 0.25f },
			{ "Floor slab", "models/quad.obj", glm::vec3{ 5.f, 1.f, 1.f }, slabBounds, FLOOR_TOP },
		};
		for (Prefab& prefab : state->prefabs) prefab.mesh = engine.renderer().loadMesh(prefab.meshPath);

		engine.addSystem(nrd::Phase::Ui, [state](nrd::Engine& engine, float) {
			if (engine.input().actionPressed("toggleEditor")) state->open = !state->open;
			if (state->open) editorWindow(engine, *state);
		});
	}
}
