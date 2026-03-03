#pragma once

#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include "PathfinderEngine.hpp"
#include "GDR2Exporter.hpp"

using namespace geode::prelude;
using namespace cocos2d;

namespace pathfinder {

    class PathfinderPanel : public CCNode {
    public:
        static PathfinderPanel* create(GJGameLevel* level);
        bool init(GJGameLevel* level);

        void onRunPathfinder(CCObject* sender);
        void onCancelPathfinder(CCObject* sender);
        void onSaveGDR2(CCObject* sender);

        void update(float dt) override;

        void cleanup();

    private:
        void updateUI();
        void extractLevelObjects();
        void showNotification(const std::string& text, bool isError = false);

        GJGameLevel* m_level = nullptr;
        std::unique_ptr<PathfinderEngine> m_engine;

        // UI elements
        CCMenu* m_menu = nullptr;
        CCMenuItemSpriteExtra* m_runButton = nullptr;
        CCMenuItemSpriteExtra* m_cancelButton = nullptr;
        CCMenuItemSpriteExtra* m_saveButton = nullptr;
        CCLabelBMFont* m_statusLabel = nullptr;
        CCLabelBMFont* m_progressLabel = nullptr;
        CCSprite* m_progressBarBg = nullptr;
        CCSprite* m_progressBarFill = nullptr;

        // Background panel
        CCScale9Sprite* m_bgPanel = nullptr;

        bool m_isRunning = false;
        bool m_hasResult = false;
        double m_lastProgress = 0.0;

        // Level data cache
        std::vector<SimObjectData> m_cachedObjects;
        double m_cachedLevelLength = 0.0;
        GameMode m_cachedStartMode = GameMode::Cube;
        SpeedSetting m_cachedStartSpeed = SpeedSetting::Normal;
        GravityDirection m_cachedStartGravity = GravityDirection::Down;
        bool m_cachedStartMini = false;
    };

} // namespace pathfinder