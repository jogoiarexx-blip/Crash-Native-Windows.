#include "entities.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

static void disableOtherEnemies(crash::EntityWorld& w, crash::RuntimeEntity* keep) {
    for (auto& e : w.items) {
        if (&e == keep) continue;
        if (crash::EntityWorld::isGenericEnemy(e.source.type) || crash::EntityWorld::isAquaticEnemy(e.source.type) ||
            e.source.type == crash::EntityWorld::EnemyType || e.source.type == crash::EntityWorld::PlantType)
            e.active = false;
    }
}

static std::vector<uint32_t> capture(const crash::Scene& scene, const crash::Player& p,
                                     const crash::EntityWorld& w, double fx, double fy) {
    int cameraX = std::clamp(int(std::lround(fx)) - 120, 0, std::max(0, p.terrain.width() - 240));
    int cameraY = std::clamp(int(std::lround(fy)) - 80, 0, std::max(0, p.terrain.height() - 160));
    auto frame = crash::GameData::viewport(scene, cameraX, cameraY, 240, 160);
    w.renderLogical(frame, 240, 160, cameraX, cameraY, false);
    return frame;
}

int main(int argc, char** argv) {
    try {
        if (argc != 3) { std::cerr << "Usage: CrashChildEffectTool ROM OUTPUT_BMP\n"; return 2; }
        crash::GameData data(crash::read(argv[1]));
        std::vector<uint32_t> strip(480 * 160, 0xff000000);
        unsigned blowCount = 0, flameCount = 0;

        {
            auto scene = data.scene(13); crash::Terrain terrain(data, scene); crash::Player p(data, scene, terrain); crash::EntityWorld w(data, scene); w.bind(p);
            auto it = std::find_if(w.items.begin(), w.items.end(), [](const crash::RuntimeEntity& e){ return e.source.type == crash::EntityWorld::BlowgunType; });
            if (it == w.items.end()) throw std::runtime_error("Blowgun not found");
            disableOtherEnemies(w, &*it); p.x = it->x + 220; p.y = it->y - 90;
            for (int i = 0; i < 1000 && w.enemyChildEffectsSpawned == 0; ++i) w.step(p);
            if (w.childEffects.empty()) throw std::runtime_error("Blowgun child did not spawn");
            blowCount = w.enemyChildEffectsSpawned; const auto fx = w.childEffects.front(); auto shot = capture(scene, p, w, fx.x, fx.y);
            for (int y=0;y<160;y++) std::copy_n(shot.begin()+y*240,240,strip.begin()+y*480);
        }
        {
            auto scene = data.scene(7); crash::Terrain terrain(data, scene); crash::Player p(data, scene, terrain); crash::EntityWorld w(data, scene); w.bind(p);
            auto it = std::find_if(w.items.begin(), w.items.end(), [](const crash::RuntimeEntity& e){ return e.source.type == crash::EntityWorld::FlamethrowerType; });
            if (it == w.items.end()) throw std::runtime_error("Flamethrower not found");
            disableOtherEnemies(w, &*it); p.x = it->x + 220; p.y = it->y - 90;
            for (int i = 0; i < 1000 && w.enemyChildEffectsSpawned == 0; ++i) w.step(p);
            if (w.childEffects.empty()) throw std::runtime_error("Flamethrower child did not spawn");
            flameCount = w.enemyChildEffectsSpawned; const auto fx = w.childEffects.front(); auto shot = capture(scene, p, w, fx.x, fx.y);
            for (int y=0;y<160;y++) std::copy_n(shot.begin()+y*240,240,strip.begin()+y*480+240);
        }
        crash::bmp(argv[2], strip, 480, 160);
        std::cout << "blowgun_children=" << blowCount << " bank=12 anim=6 speed=4 flame_children=" << flameCount << " bank=23 anim=4 lifetime=24\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
