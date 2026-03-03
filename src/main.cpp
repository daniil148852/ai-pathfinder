#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include "PathfinderUI.hpp"

using namespace geode::prelude;
using namespace cocos2d;

class $modify(PathfinderLevelInfoLayer, LevelInfoLayer) {
    struct Fields {
        pathfinder::PathfinderPanel* pathfinderPanel = nullptr;
    };

    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;

        // Get the level for our pathfinder
        auto pl = m_fields->pathfinderPanel = pathfinder::PathfinderPanel::create(level);
        if (!pl) {
            log::error("Failed to create PathfinderPanel");
            return true; // Don't prevent level info from loading
        }

        // Position the panel in the level info scene
        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Place it on the right side of the screen, below center
        pl->setPosition({winSize.width - 155.0f, winSize.height * 0.35f});
        pl->setZOrder(100);

        // Add to this layer
        this->addChild(pl);

        // Add a subtle entrance animation
        pl->setScale(0.0f);
        pl->setOpacity(0);
        pl->runAction(CCSequence::create(
            CCDelayTime::create(0.3f),
            CCSpawn::create(
                CCEaseBackOut::create(CCScaleTo::create(0.4f, 1.0f)),
                CCFadeIn::create(0.3f),
                nullptr
            ),
            nullptr
        ));

        log::info("Pathfinder: Panel added to LevelInfoLayer");

        return true;
    }

    void onBack(CCObject* sender) {
        // Clean up pathfinder before leaving
        if (m_fields->pathfinderPanel) {
            m_fields->pathfinderPanel->cleanup();
            m_fields->pathfinderPanel = nullptr;
        }

        LevelInfoLayer::onBack(sender);
    }

    // Also handle if the layer is being destroyed by other means
    virtual ~PathfinderLevelInfoLayer() {
        if (m_fields->pathfinderPanel) {
            m_fields->pathfinderPanel->cleanup();
            m_fields->pathfinderPanel = nullptr;
        }
    }
};

// Additional safety: handle when PlayLayer is entered
// (level info layer goes away)
#include <Geode/modify/PlayLayer.hpp>

class $modify(PathfinderPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        // The LevelInfoLayer cleanup will handle pathfinder cancellation
        return PlayLayer::init(level, useReplay, dontCreateObjects);
    }
};