#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "render/logger.h"
#include "render/renderer.h"
#include "render/async_resource_loader.h"
#include "render/texture_loader.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/ecs/systems.h"
#include "render/ecs/sprite_animation_script_registry.h"
#include "render/debug/sprite_animation_debug_panel.h"

using namespace Render;
using namespace Render::ECS;

namespace {

void RegisterAnimationScripts() {
    static bool registered = false;
    if (registered) {
        return;
    }
    SpriteAnimationScriptRegistry::Register("Anim.OnIdleEnter",
        [](EntityID entity, const SpriteAnimationEvent& evt, SpriteAnimationComponent&) {
            Logger::GetInstance().InfoFormat("[Script] Entity %u entered idle (clip=%s, frame=%d)",
                                             entity.index, evt.clip.c_str(), evt.frameIndex);
        });

    SpriteAnimationScriptRegistry::Register("Anim.PlayFootstep",
        [](EntityID entity, const SpriteAnimationEvent& evt, SpriteAnimationComponent&) {
            Logger::GetInstance().InfoFormat("[Script] Entity %u footstep event (clip=%s, frame=%d)",
                                             entity.index, evt.clip.c_str(), evt.frameIndex);
        });

    SpriteAnimationScriptRegistry::Register("Anim.OnAttackFinished",
        [](EntityID entity, const SpriteAnimationEvent& evt, SpriteAnimationComponent& component) {
            Logger::GetInstance().InfoFormat("[Script] Entity %u attack finished (clip=%s)", entity.index, evt.clip.c_str());
            component.SetTrigger("attackFinished");
        });

    SpriteAnimationScriptRegistry::Register("Anim.EmitAttackImpact",
        [](EntityID entity, const SpriteAnimationEvent& evt, SpriteAnimationComponent&) {
            Logger::GetInstance().InfoFormat("[Script] Entity %u attack impact (clip=%s, frame=%d)",
                                             entity.index, evt.clip.c_str(), evt.frameIndex);
        });

    SpriteAnimationScriptRegistry::Register("Anim.OnJumpEnter",
        [](EntityID entity, const SpriteAnimationEvent&, SpriteAnimationComponent& component) {
            Logger::GetInstance().InfoFormat("[Script] Entity %u jump start", entity.index);
            component.SetBoolParameter("isGrounded", false);
            component.SetBoolParameter("isAirborne", true);
        });

    SpriteAnimationScriptRegistry::Register("Anim.OnJumpExit",
        [](EntityID entity, const SpriteAnimationEvent&, SpriteAnimationComponent& component) {
            Logger::GetInstance().InfoFormat("[Script] Entity %u jump end", entity.index);
            component.SetBoolParameter("isGrounded", true);
            component.SetBoolParameter("isAirborne", false);
        });

    SpriteAnimationScriptRegistry::Register("Anim.OnHitEnter",
        [](EntityID entity, const SpriteAnimationEvent&, SpriteAnimationComponent& component) {
            Logger::GetInstance().InfoFormat("[Script] Entity %u hit reaction enter", entity.index);
            component.SetBoolParameter("isStunned", true);
        });

    SpriteAnimationScriptRegistry::Register("Anim.OnHitExit",
        [](EntityID entity, const SpriteAnimationEvent&, SpriteAnimationComponent& component) {
            Logger::GetInstance().InfoFormat("[Script] Entity %u hit reaction exit", entity.index);
            component.SetBoolParameter("isStunned", false);
        });

    SpriteAnimationScriptRegistry::Register("Anim.OnHitImpact",
        [](EntityID entity, const SpriteAnimationEvent& evt, SpriteAnimationComponent&) {
            Logger::GetInstance().InfoFormat("[Script] Entity %u hit impact (clip=%s, frame=%d)",
                                             entity.index, evt.clip.c_str(), evt.frameIndex);
        });

    registered = true;
}

SpriteAnimationClip BuildClip(const std::vector<Rect>& frames, float duration, SpritePlaybackMode mode, bool loop = true) {
    SpriteAnimationClip clip{};
    clip.frames = frames;
    clip.frameDuration = duration;
    clip.playbackMode = mode;
    clip.loop = loop;
    return clip;
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    RegisterAnimationScripts();

    Logger::GetInstance().SetLogToFile(false);
    Logger::GetInstance().Info("=== Sprite State Machine Test ===");

    auto renderer = std::make_unique<Renderer>();
    if (!renderer->Initialize("Sprite State Machine Test", 800, 600)) {
        Logger::GetInstance().Error("[SpriteStateMachineTest] Renderer initialization failed");
        return -1;
    }

    AsyncResourceLoader::GetInstance().Initialize();

    auto world = std::make_shared<World>();
    world->Initialize();
    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<SpriteRenderComponent>();
    world->RegisterComponent<SpriteAnimationComponent>();
    world->RegisterSystem<TransformSystem>();
    world->RegisterSystem<SpriteAnimationSystem>();
    world->RegisterSystem<SpriteRenderSystem>(renderer.get());
    world->RegisterSystem<UniformSystem>(renderer.get());
    world->PostInitialize();

    const std::string textureName = "sprite_state_machine_test_texture";
    auto texture = TextureLoader::GetInstance().LoadTexture(textureName, "textures/test.jpg", true);
    if (!texture) {
        Logger::GetInstance().Error("[SpriteStateMachineTest] Failed to load test texture");
        return -1;
    }

    SpriteRenderComponent spriteTemplate;
    spriteTemplate.texture = texture;
    spriteTemplate.textureName = textureName;
    spriteTemplate.size = Vector2(200.0f, 200.0f);
    spriteTemplate.screenSpace = true;
    spriteTemplate.layerID = 800;
    spriteTemplate.resourcesLoaded = true;

    SpriteAnimationComponent animTemplate;
    animTemplate.clips["idle_clip"] = BuildClip(
        {Rect(0, 0, 64, 64), Rect(64, 0, 64, 64)}, 0.5f, SpritePlaybackMode::Loop, true);
    animTemplate.clips["run_clip"] = BuildClip(
        {Rect(0, 0, 64, 64), Rect(64, 0, 64, 64), Rect(128, 0, 64, 64)}, 0.18f, SpritePlaybackMode::Loop, true);
    animTemplate.clips["attack_clip"] = BuildClip(
        {Rect(0, 0, 64, 64), Rect(64, 0, 64, 64), Rect(128, 0, 64, 64)}, 0.12f, SpritePlaybackMode::Once, false);
    animTemplate.clips["jump_clip"] = BuildClip(
        {Rect(0, 0, 64, 64), Rect(64, 0, 64, 64), Rect(128, 0, 64, 64)}, 0.16f, SpritePlaybackMode::Once, false);
    animTemplate.clips["hit_clip"] = BuildClip(
        {Rect(0, 0, 64, 64), Rect(64, 0, 64, 64), Rect(128, 0, 64, 64), Rect(64, 0, 64, 64)}, 0.09f, SpritePlaybackMode::PingPong, false);

    animTemplate.AddState({
        .name = "idle",
        .clip = "idle_clip",
        .playbackSpeed = 1.0f,
        .playbackMode = SpritePlaybackMode::Loop,
        .resetOnEnter = true,
        .onEnterScripts = {"Anim.OnIdleEnter"}
    });

    animTemplate.AddState({
        .name = "run",
        .clip = "run_clip",
        .playbackSpeed = 1.0f,
        .playbackMode = SpritePlaybackMode::Loop,
        .resetOnEnter = true,
        .onEnterScripts = {},
        .onExitScripts = {}
    });

    animTemplate.AddState({
        .name = "attack",
        .clip = "attack_clip",
        .playbackSpeed = 1.2f,
        .playbackMode = SpritePlaybackMode::Once,
        .resetOnEnter = true,
        .onEnterScripts = {},
        .onExitScripts = {"Anim.OnAttackFinished"}
    });

    animTemplate.AddState({
        .name = "jump",
        .clip = "jump_clip",
        .playbackSpeed = 1.0f,
        .playbackMode = SpritePlaybackMode::Once,
        .resetOnEnter = true,
        .onEnterScripts = {"Anim.OnJumpEnter"},
        .onExitScripts = {"Anim.OnJumpExit"}
    });

    animTemplate.AddState({
        .name = "hit",
        .clip = "hit_clip",
        .playbackSpeed = 1.0f,
        .playbackMode = SpritePlaybackMode::PingPong,
        .resetOnEnter = true,
        .onEnterScripts = {"Anim.OnHitEnter"},
        .onExitScripts = {"Anim.OnHitExit"}
    });

    animTemplate.AddTransition({
        .fromState = "idle",
        .toState = "run",
        .conditions = {
            {.type = SpriteAnimationTransitionCondition::Type::FloatGreater,
             .parameter = "speed",
             .threshold = 0.2f}
        }
    });

    animTemplate.AddTransition({
        .fromState = "run",
        .toState = "idle",
        .conditions = {
            {.type = SpriteAnimationTransitionCondition::Type::FloatLess,
             .parameter = "speed",
             .threshold = 0.15f}
        }
    });

    animTemplate.AddTransition({
        .fromState = "run",
        .toState = "attack",
        .conditions = {
            {.type = SpriteAnimationTransitionCondition::Type::Trigger,
             .parameter = "attackTrigger"}
        },
        .once = true
    });

    animTemplate.AddTransition({
        .fromState = "attack",
        .toState = "idle",
        .conditions = {
            {.type = SpriteAnimationTransitionCondition::Type::OnEvent,
             .eventType = SpriteAnimationEvent::Type::ClipCompleted,
             .eventClip = "attack_clip"}
        }
    });

    animTemplate.AddTransition({
        .fromState = "",
        .toState = "idle",
        .conditions = {
            {.type = SpriteAnimationTransitionCondition::Type::Trigger,
             .parameter = "attackFinished"}
        }
    });

    animTemplate.AddTransition({
        .fromState = "idle",
        .toState = "jump",
        .conditions = {
            {.type = SpriteAnimationTransitionCondition::Type::Trigger,
             .parameter = "jumpTrigger"}
        }
    });

    animTemplate.AddTransition({
        .fromState = "run",
        .toState = "jump",
        .conditions = {
            {.type = SpriteAnimationTransitionCondition::Type::Trigger,
             .parameter = "jumpTrigger"}
        }
    });

    animTemplate.AddTransition({
        .fromState = "jump",
        .toState = "run",
        .conditions = {
            {.type = SpriteAnimationTransitionCondition::Type::OnEvent,
             .eventType = SpriteAnimationEvent::Type::ClipCompleted,
             .eventClip = "jump_clip"},
            {.type = SpriteAnimationTransitionCondition::Type::FloatGreater,
             .parameter = "speed",
             .threshold = 0.2f}
        }
    });

    animTemplate.AddTransition({
        .fromState = "jump",
        .toState = "idle",
        .conditions = {
            {.type = SpriteAnimationTransitionCondition::Type::OnEvent,
             .eventType = SpriteAnimationEvent::Type::ClipCompleted,
             .eventClip = "jump_clip"},
            {.type = SpriteAnimationTransitionCondition::Type::FloatLess,
             .parameter = "speed",
             .threshold = 0.2f}
        }
    });

    animTemplate.AddTransition({
        .fromState = "",
        .toState = "hit",
        .conditions = {
            {.type = SpriteAnimationTransitionCondition::Type::Trigger,
             .parameter = "hitTrigger"}
        }
    });

    animTemplate.AddTransition({
        .fromState = "hit",
        .toState = "run",
        .conditions = {
            {.type = SpriteAnimationTransitionCondition::Type::Trigger,
             .parameter = "recoverTrigger"},
            {.type = SpriteAnimationTransitionCondition::Type::FloatGreater,
             .parameter = "speed",
             .threshold = 0.2f}
        }
    });

    animTemplate.AddTransition({
        .fromState = "hit",
        .toState = "idle",
        .conditions = {
            {.type = SpriteAnimationTransitionCondition::Type::Trigger,
             .parameter = "recoverTrigger"},
            {.type = SpriteAnimationTransitionCondition::Type::FloatLess,
             .parameter = "speed",
             .threshold = 0.2f}
        }
    });

    animTemplate.AddScriptBinding({
        .eventType = SpriteAnimationEvent::Type::FrameChanged,
        .clip = "run_clip",
        .frameIndex = 1,
        .scriptName = "Anim.PlayFootstep"
    });

    animTemplate.AddScriptBinding({
        .eventType = SpriteAnimationEvent::Type::FrameChanged,
        .clip = "attack_clip",
        .frameIndex = 1,
        .scriptName = "Anim.EmitAttackImpact"
    });

    animTemplate.AddScriptBinding({
        .eventType = SpriteAnimationEvent::Type::FrameChanged,
        .clip = "hit_clip",
        .frameIndex = 2,
        .scriptName = "Anim.OnHitImpact"
    });

    animTemplate.SetBoolParameter("isGrounded", true);
    animTemplate.SetBoolParameter("isAirborne", false);
    animTemplate.SetBoolParameter("isStunned", false);
    animTemplate.SetFloatParameter("speed", 0.0f);

    animTemplate.SetDefaultState("idle");
    animTemplate.AddEventListener([](EntityID id, const SpriteAnimationEvent& evt) {
        Logger::GetInstance().InfoFormat("[Event] Entity %u event %d clip=%s frame=%d",
                                         id.index, static_cast<int>(evt.type),
                                         evt.clip.c_str(), evt.frameIndex);
    });

    struct ActorColors {
        Color idle{0.8f, 1.0f, 0.6f, 1.0f};
        Color run{0.4f, 0.8f, 1.0f, 1.0f};
        Color attack{1.0f, 0.4f, 0.4f, 1.0f};
        Color jump{0.7f, 0.5f, 1.0f, 1.0f};
        Color hit{1.0f, 0.85f, 0.3f, 1.0f};
    };

    struct ActorMotion {
        float runAmplitude = 40.0f;
        float runFrequency = 2.0f;
        float jumpHeight = 80.0f;
        float jumpHorizontalAmplitude = 20.0f;
        float jumpHorizontalFrequency = 1.5f;
        float hitShakeAmplitude = 25.0f;
        float hitShakeFrequency = 25.0f;
    };

    struct ActorTimeline {
        float startOffset = 0.0f;
        float runStart = -1.0f;
        float runStop = -1.0f;
        float runResumeStart = -1.0f;
        float runResumeStop = -1.0f;
        float slowRunStart = -1.0f;
        float slowRunStop = -1.0f;
        float slowRunSpeed = 0.3f;
        float attackTime = -1.0f;
        float jumpTime = -1.0f;
        float hitTime = -1.0f;
        float recoverDelay = 0.8f;
        float jumpDuration = 0.6f;
    };

    struct ActorScenario {
        EntityID entity{};
        std::string name;
        Vector3 basePosition{};
        ActorColors colors{};
        ActorMotion motion{};
        ActorTimeline timeline{};
        bool attackTriggered = false;
        bool jumpTriggered = false;
        bool hitTriggered = false;
        bool recoverQueued = false;
    };

    auto spawnActor = [&](const std::string& name,
                          const Vector3& position,
                          const ActorColors& colors,
                          const ActorMotion& motion,
                          const ActorTimeline& timeline) -> ActorScenario {
        EntityDescriptor entityDesc{};
        entityDesc.name = name;
        EntityID newEntity = world->CreateEntity(entityDesc);

        TransformComponent actorTransform;
        actorTransform.SetPosition(position);
        actorTransform.SetScale(Vector3(1.0f, 1.0f, 1.0f));
        world->AddComponent(newEntity, actorTransform);

        SpriteRenderComponent actorSprite = spriteTemplate;
        actorSprite.tintColor = colors.idle;
        world->AddComponent(newEntity, actorSprite);

        SpriteAnimationComponent actorAnim = animTemplate;
        actorAnim.AddEventListener([name](EntityID id, const SpriteAnimationEvent& evt) {
            Logger::GetInstance().DebugFormat("[Actor:%s] entity=%u event=%d clip=%s frame=%d",
                                              name.c_str(),
                                              id.index,
                                              static_cast<int>(evt.type),
                                              evt.clip.c_str(),
                                              evt.frameIndex);
        });
        actorAnim.Play("idle_clip");
        world->AddComponent(newEntity, actorAnim);

        return ActorScenario{
            .entity = newEntity,
            .name = name,
            .basePosition = position,
            .colors = colors,
            .motion = motion,
            .timeline = timeline
        };
    };

    std::vector<ActorScenario> actors;

    ActorColors primaryColors{
        .idle = Color(0.80f, 0.95f, 0.70f, 1.0f),
        .run = Color(0.40f, 0.80f, 1.00f, 1.0f),
        .attack = Color(1.00f, 0.45f, 0.40f, 1.0f),
        .jump = Color(0.70f, 0.50f, 1.00f, 1.0f),
        .hit = Color(1.00f, 0.85f, 0.35f, 1.0f)
    };
    ActorMotion primaryMotion{
        .runAmplitude = 42.0f,
        .runFrequency = 2.0f,
        .jumpHeight = 85.0f,
        .jumpHorizontalAmplitude = 22.0f,
        .jumpHorizontalFrequency = 1.6f,
        .hitShakeAmplitude = 24.0f,
        .hitShakeFrequency = 24.0f
    };
    ActorTimeline primaryTimeline{
        .startOffset = 0.0f,
        .runStart = 1.0f,
        .runStop = 3.5f,
        .runResumeStart = 6.4f,
        .runResumeStop = 7.2f,
        .slowRunStart = 7.2f,
        .slowRunStop = 8.4f,
        .slowRunSpeed = 0.3f,
        .attackTime = 2.5f,
        .jumpTime = 4.0f,
        .hitTime = 5.5f,
        .recoverDelay = 0.8f,
        .jumpDuration = 0.6f
    };
    actors.push_back(spawnActor("Actor.Primary",
                                Vector3(220.0f, 260.0f, 0.0f),
                                primaryColors,
                                primaryMotion,
                                primaryTimeline));

    ActorColors scoutColors{
        .idle = Color(0.9f, 0.9f, 1.0f, 1.0f),
        .run = Color(0.5f, 0.9f, 0.6f, 1.0f),
        .attack = Color(1.0f, 0.6f, 0.4f, 1.0f),
        .jump = Color(0.6f, 0.7f, 1.0f, 1.0f),
        .hit = Color(1.0f, 0.9f, 0.4f, 1.0f)
    };
    ActorMotion scoutMotion{
        .runAmplitude = 55.0f,
        .runFrequency = 3.1f,
        .jumpHeight = 65.0f,
        .jumpHorizontalAmplitude = 30.0f,
        .jumpHorizontalFrequency = 2.4f,
        .hitShakeAmplitude = 18.0f,
        .hitShakeFrequency = 32.0f
    };
    ActorTimeline scoutTimeline{
        .startOffset = 0.9f,
        .runStart = 0.2f,
        .runStop = 4.8f,
        .runResumeStart = 5.5f,
        .runResumeStop = 7.6f,
        .slowRunStart = 7.6f,
        .slowRunStop = 8.8f,
        .slowRunSpeed = 0.5f,
        .attackTime = -1.0f,
        .jumpTime = 2.6f,
        .hitTime = 6.1f,
        .recoverDelay = 0.6f,
        .jumpDuration = 0.55f
    };
    actors.push_back(spawnActor("Actor.Scout",
                                Vector3(420.0f, 280.0f, 0.0f),
                                scoutColors,
                                scoutMotion,
                                scoutTimeline));

    ActorColors defenderColors{
        .idle = Color(0.95f, 0.85f, 0.95f, 1.0f),
        .run = Color(0.65f, 0.75f, 0.95f, 1.0f),
        .attack = Color(1.0f, 0.55f, 0.55f, 1.0f),
        .jump = Color(0.75f, 0.55f, 1.0f, 1.0f),
        .hit = Color(1.0f, 0.9f, 0.5f, 1.0f)
    };
    ActorMotion defenderMotion{
        .runAmplitude = 28.0f,
        .runFrequency = 1.4f,
        .jumpHeight = 50.0f,
        .jumpHorizontalAmplitude = 12.0f,
        .jumpHorizontalFrequency = 1.0f,
        .hitShakeAmplitude = 35.0f,
        .hitShakeFrequency = 18.0f
    };
    ActorTimeline defenderTimeline{
        .startOffset = 1.6f,
        .runStart = 4.4f,
        .runStop = 6.3f,
        .runResumeStart = -1.0f,
        .runResumeStop = -1.0f,
        .slowRunStart = 6.3f,
        .slowRunStop = 8.5f,
        .slowRunSpeed = 0.4f,
        .attackTime = 3.2f,
        .jumpTime = -1.0f,
        .hitTime = 2.1f,
        .recoverDelay = 1.2f,
        .jumpDuration = 0.7f
    };
    actors.push_back(spawnActor("Actor.Defender",
                                Vector3(620.0f, 245.0f, 0.0f),
                                defenderColors,
                                defenderMotion,
                                defenderTimeline));

    bool running = true;
    float runtime = 0.0f;
    const float kMaxRuntime = 11.0f;

#if defined(DEBUG) || defined(_DEBUG)
    SpriteAnimationDebugPanel debugPanel;
    debugPanel.WatchAllEntities(true);
    debugPanel.SetMaxEventsPerEntity(4);
    double debugPanelTimer = 0.0;
#endif

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        renderer->BeginFrame();
        float deltaTime = renderer->GetDeltaTime();
        runtime += deltaTime;

        auto inRange = [](float value, float start, float stop) {
            return start >= 0.0f && stop >= 0.0f && value >= start && value < stop;
        };

        for (auto& actor : actors) {
            auto& animRef = world->GetComponent<SpriteAnimationComponent>(actor.entity);
            auto& spriteRef = world->GetComponent<SpriteRenderComponent>(actor.entity);
            auto& transformRef = world->GetComponent<TransformComponent>(actor.entity);

            float localTime = runtime - actor.timeline.startOffset;
            if (localTime < 0.0f) {
                animRef.SetFloatParameter("speed", 0.0f);
                spriteRef.tintColor = actor.colors.idle;
                transformRef.transform->SetPosition(actor.basePosition);
                continue;
            }

            float desiredSpeed = 0.0f;
            if (inRange(localTime, actor.timeline.runStart, actor.timeline.runStop) ||
                inRange(localTime, actor.timeline.runResumeStart, actor.timeline.runResumeStop)) {
                desiredSpeed = 1.0f;
            }
            if (inRange(localTime, actor.timeline.slowRunStart, actor.timeline.slowRunStop)) {
                desiredSpeed = actor.timeline.slowRunSpeed;
            }
            animRef.SetFloatParameter("speed", desiredSpeed);

            if (!actor.attackTriggered && actor.timeline.attackTime >= 0.0f && localTime >= actor.timeline.attackTime) {
                Logger::GetInstance().InfoFormat("[Test] %s trigger attack", actor.name.c_str());
                animRef.SetTrigger("attackTrigger");
                actor.attackTriggered = true;
            }
            if (!actor.jumpTriggered && actor.timeline.jumpTime >= 0.0f && localTime >= actor.timeline.jumpTime) {
                Logger::GetInstance().InfoFormat("[Test] %s trigger jump", actor.name.c_str());
                animRef.SetTrigger("jumpTrigger");
                actor.jumpTriggered = true;
            }
            if (!actor.hitTriggered && actor.timeline.hitTime >= 0.0f && localTime >= actor.timeline.hitTime) {
                Logger::GetInstance().InfoFormat("[Test] %s trigger hit reaction", actor.name.c_str());
                animRef.SetTrigger("hitTrigger");
                actor.hitTriggered = true;
            }

            if (animRef.currentState == "hit") {
                if (!actor.recoverQueued && animRef.stateTime >= actor.timeline.recoverDelay) {
                    Logger::GetInstance().InfoFormat("[Test] %s trigger recover", actor.name.c_str());
                    animRef.SetTrigger("recoverTrigger");
                    actor.recoverQueued = true;
                }
            } else {
                actor.recoverQueued = false;
            }
        }

        world->Update(deltaTime);

#if defined(DEBUG) || defined(_DEBUG)
        debugPanelTimer += deltaTime;
        if (debugPanelTimer >= 2.0) {
            Logger::GetInstance().Info("[DebugPanel] Sprite animation snapshot");
            debugPanel.RenderToLogger();
            debugPanelTimer = 0.0;
        }
#endif

        for (auto& actor : actors) {
            auto& animRef = world->GetComponent<SpriteAnimationComponent>(actor.entity);
            auto& spriteRef = world->GetComponent<SpriteRenderComponent>(actor.entity);
            auto& transformRef = world->GetComponent<TransformComponent>(actor.entity);

            float localTime = runtime - actor.timeline.startOffset;
            if (localTime < 0.0f) {
                continue;
            }

            constexpr float kPi = 3.14159265358979323846f;
            float offsetX = 0.0f;
            float offsetY = 0.0f;
            Color tint = actor.colors.idle;

            if (animRef.currentState == "run") {
                tint = actor.colors.run;
                offsetX = std::sin((runtime + actor.timeline.startOffset) * actor.motion.runFrequency) *
                          actor.motion.runAmplitude;
            } else if (animRef.currentState == "attack") {
                tint = actor.colors.attack;
            } else if (animRef.currentState == "jump") {
                tint = actor.colors.jump;
                float jumpDuration = std::max(actor.timeline.jumpDuration, 0.2f);
                float normalized = std::clamp(animRef.stateTime / jumpDuration, 0.0f, 1.0f);
                offsetY = -std::sin(normalized * kPi) * actor.motion.jumpHeight;
                offsetX = std::sin((runtime + actor.timeline.startOffset) * actor.motion.jumpHorizontalFrequency) *
                          actor.motion.jumpHorizontalAmplitude;
            } else if (animRef.currentState == "hit") {
                tint = actor.colors.hit;
                float recoverWindow = std::max(actor.timeline.recoverDelay, 0.2f);
                float shakeFactor = std::clamp(1.0f - animRef.stateTime / recoverWindow, 0.0f, 1.0f);
                offsetX = std::sin((runtime + actor.timeline.startOffset) * actor.motion.hitShakeFrequency) *
                          actor.motion.hitShakeAmplitude * shakeFactor;
            }

            spriteRef.tintColor = tint;
            transformRef.transform->SetPosition(actor.basePosition + Vector3(offsetX, offsetY, 0.0f));
        }

        renderer->Clear(true, true, false);
        renderer->FlushRenderQueue();
        renderer->EndFrame();
        renderer->Present();

        if (runtime >= kMaxRuntime) {
            running = false;
        }

        SDL_Delay(16);
    }

    Logger::GetInstance().Info("[SpriteStateMachineTest] Main loop exited");
    world->Shutdown();
    AsyncResourceLoader::GetInstance().Shutdown();
    renderer->Shutdown();
    Logger::GetInstance().Info("=== Sprite State Machine Test Completed ===");
    return 0;
}


