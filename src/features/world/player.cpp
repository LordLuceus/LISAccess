#include "features/world/session.h"
#include "features/world/menu_state.h"
#include "platform/log.h"
#include "platform/foreground.h"
#include "engine/abi.h"
#include "engine/build_profile.h"
#include "features/navigation/collision.h"
#include <algorithm>
#include <cmath>
#include <cctype>
namespace lis::world {
using engine::Copy;
using engine::Read;
using engine::Array;
bool GatePlayer(Session& session,Snapshot& s,uintptr_t& pawn,bool refreshDiscovery) {
    session.liveController=session.liveWorld=session.liveFocus=0;
    if (refreshDiscovery ? !Discover(session) : !session.ready) { s.reason = "discovering bounded world metadata"; return false; }
    for (uintptr_t m : session.menus) if (BlockingMenu(session.reflection,m)) {
        s.reason = "pause, diary or out-game menu"; return false;
    }
    uintptr_t controller = 0, world = 0;
    pawn = 0;
    for (uintptr_t c : session.controllers) {
        uintptr_t p = 0, ex = 0, back = 0, w = 0;
        if (!session.reflection.registry.Instance(c) || !session.reflection.registry.IsA(c,"WhatIfController") || !session.reflection.Value(c, "Controller", "Pawn", "ObjectProperty", p) || !session.reflection.registry.Instance(p) || !session.reflection.registry.IsA(p,"Pawn") ||
            !session.reflection.Value(c, "WhatIfController", "ExPawn", "ObjectProperty", ex) || ex != p ||
            !session.reflection.Value(p, "Pawn", "Controller", "ObjectProperty", back) || back != c ||
            !Copy(&w, p + 0xd8, 4) || !session.reflection.registry.Registered(w) || Read<uintptr_t>(c + 0xd8) != w) continue;
        if (controller) { s.reason = "ambiguous local controller"; return false; }
        controller = c; pawn = p; world = w;
    }
    if (!controller) { s.reason = "no verified live player"; return false; }
    session.liveController=controller;session.liveWorld=world;
    uintptr_t hud=0,debugMenu=0;
    if(!session.reflection.Value(controller,"PlayerController","myHUD","ObjectProperty",hud)||!session.reflection.registry.Instance(hud)||!session.reflection.registry.IsA(hud,"ExampleHUD") ||
        !session.reflection.Value(hud,"ExampleHUD","myDebugMenu","ObjectProperty",debugMenu)) {
        s.reason="HUD input context unavailable";return false;
    }
    // D-pad commands also belong to the native debug menu. Never run scanner
    // controls alongside it, even if ordinary traversal flags remain enabled.
    if(debugMenu&&(!session.reflection.registry.Instance(debugMenu)||!session.reflection.registry.IsA(debugMenu,"DebugMenu") ||
        !session.reflection.ClearBool(debugMenu,"DebugMenu","bActive")||!session.reflection.ClearBool(debugMenu,"DebugMenu","bShow"))) {
        s.reason="native debug menu active";return false;
    }
    unsigned char gameplay = 0, move = 1, look = 1;
    uintptr_t pauser = 0;
    if (!session.reflection.Value(controller, "WhatIfController", "CurrentGameplayType", "ByteProperty", gameplay) || gameplay != 1 ||
        !session.reflection.Value(world, "WorldInfo", "Pauser", "ObjectProperty", pauser) || pauser ||
        !session.reflection.ClearBool(controller, "WhatIfController", "bGameplayPause") ||
        !session.reflection.ClearBool(controller, "WhatIfController", "PlayerControlDisabled") ||
        !session.reflection.ClearBool(controller, "PlayerController", "bCinematicMode") ||
        !session.reflection.Value(controller, "PlayerController", "bIgnoreMoveInput", "ByteProperty", move) || move ||
        !session.reflection.Value(controller, "PlayerController", "bIgnoreLookInput", "ByteProperty", look) || look) {
        s.reason = "traversal controls unavailable"; return false;
    }
    uintptr_t cylinder = 0, camera = 0;
    int cache = 0, pov = 0, rotation = 0;
    // Struct sizes are validated by metadata rather than assuming stock SDK packing.

    int yaw = 0;
    if (!Copy(&s.player, pawn + 0x54, sizeof(Vec3)) || !Finite(s.player) ||
        !session.reflection.Value(pawn, "Actor", "CollisionComponent", "ComponentProperty", cylinder) || !session.reflection.registry.Registered(cylinder) || !session.reflection.registry.IsA(cylinder, "CylinderComponent") ||
        !session.reflection.Value(cylinder, "CylinderComponent", "CollisionRadius", "FloatProperty", s.radius) ||
        !session.reflection.Value(cylinder, "CylinderComponent", "CollisionHeight", "FloatProperty", s.halfHeight) ||
        !std::isfinite(s.radius) || !std::isfinite(s.halfHeight) || s.radius < 1 || s.radius > 256 || s.halfHeight < s.radius || s.halfHeight > 512 ||
        !session.reflection.Value(controller, "WhatIfController", "ExCamera", "ObjectProperty", camera) || !session.reflection.registry.Registered(camera) || !session.reflection.registry.IsA(camera, "Camera") ||
        !session.reflection.StructOffset("Camera", "CameraCache", cache) || !session.reflection.StructOffset("TCameraCache", "POV", pov) ||
        !session.reflection.Offset("TPOV", "Rotation", "StructProperty", 12, rotation) ||
        !Copy(&yaw, camera + cache + pov + rotation + 4, 4)) {
        s.reason = "invalid player cylinder or camera metadata"; return false;
    }
    s.cameraYaw = static_cast<float>(yaw % 65536) * (2 * Pi / 65536);
    return true;
}
}
