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

        auto pl = m_fields->pathfinderPanel = pathfinder::PathfinderPanel::create(level);
        if (!pl) {
            log::error("Failed to create PathfinderPanel");
            return true;
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        pl->setPosition({winSize.width - 155.0f, winSize.height * 0.35f});
        pl->setZOrder(100);

        this->addChild(pl);

        // Entrance animation (use setScale only — CCNode has no setOpacity)
        pl->setScale(0.0f);
        pl->runAction(CCSequence::create(
            CCDelayTime::create(0.3f),
            CCEaseBackOut::create(CCScaleTo::create(0.4f, 1.0f)),
            nullptr
        ));

        log::info("Pathfinder: Panel added to LevelInfoLayer");

        return true;
    }

    void onBack(CCObject* sender) {
        if (m_fields->pathfinderPanel) {
            m_fields->pathfinderPanel->cleanup();
            m_fields->pathfinderPanel = nullptr;
        }

        LevelInfoLayer::onBack(sender);
    }
};