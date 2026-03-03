#include "PathfinderUI.hpp"
#include <Geode/Geode.hpp>
#include <sstream>

using namespace geode::prelude;

namespace pathfinder {

    // ─── Object Classification ─────────────────────────────────────────

    void classifyObject(SimObjectData& obj) {
        int id = obj.objectID;

        if (obj.width <= 0.0) obj.width = 30.0;
        if (obj.height <= 0.0) obj.height = 30.0;

        obj.portalType = 0;
        obj.speedType = -1;
        obj.gravityType = -1;
        obj.padType = -1;
        obj.orbType = -1;

        bool isSpike = (id == 8 || id == 39 || id == 103 || id == 392 ||
                        id == 421 || id == 422 || id == 9 || id == 61 ||
                        id == 243 || id == 244 || id == 363 || id == 364 ||
                        id == 365 || id == 366 || id == 446 || id == 447);

        bool isSaw = (id == 88 || id == 89 || id == 98 || id == 397 ||
                      id == 398 || id == 399 || id == 1705 || id == 1706 ||
                      id == 1707 || id == 1708 || id == 1709 || id == 1710);

        if (isSpike) {
            obj.isHazard = true;
            obj.width = 25.0;
            obj.height = 25.0;
            return;
        }

        if (isSaw) {
            obj.isHazard = true;
            obj.width = 40.0;
            obj.height = 40.0;
            return;
        }

        if (id == 280 || id == 281 || id == 282 || id == 283 ||
            id == 462 || id == 463 || id == 464 || id == 465 ||
            id == 667 || id == 668 || id == 669 || id == 670) {
            obj.isSlope = true;
            obj.isSolid = true;
            if (id == 280 || id == 462 || id == 667) {
                obj.slopeAngle = 45.0;
            } else if (id == 281 || id == 463 || id == 668) {
                obj.slopeAngle = -45.0;
            } else if (id == 282 || id == 464 || id == 669) {
                obj.slopeAngle = 26.57;
            } else {
                obj.slopeAngle = -26.57;
            }
            obj.slopeAngle += obj.rotation;
            return;
        }

        if (id == 12 || id == 745) { obj.isPortal = true; obj.portalType = 1; obj.height = 60.0; return; }
        if (id == 13 || id == 746) { obj.isPortal = true; obj.portalType = 2; obj.height = 60.0; return; }
        if (id == 47 || id == 747) { obj.isPortal = true; obj.portalType = 3; obj.height = 60.0; return; }
        if (id == 111 || id == 748) { obj.isPortal = true; obj.portalType = 4; obj.height = 60.0; return; }
        if (id == 660 || id == 749) { obj.isPortal = true; obj.portalType = 5; obj.height = 60.0; return; }
        if (id == 750) { obj.isPortal = true; obj.portalType = 6; obj.height = 60.0; return; }
        if (id == 751) { obj.isPortal = true; obj.portalType = 7; obj.height = 60.0; return; }
        if (id == 1331) { obj.isPortal = true; obj.portalType = 8; obj.height = 60.0; return; }

        if (id == 200) { obj.isPortal = true; obj.speedType = 0; obj.height = 60.0; return; }
        if (id == 201) { obj.isPortal = true; obj.speedType = 1; obj.height = 60.0; return; }
        if (id == 202) { obj.isPortal = true; obj.speedType = 2; obj.height = 60.0; return; }
        if (id == 203) { obj.isPortal = true; obj.speedType = 3; obj.height = 60.0; return; }
        if (id == 1334) { obj.isPortal = true; obj.speedType = 4; obj.height = 60.0; return; }
        if (id == 1587) { obj.isPortal = true; obj.speedType = 5; obj.height = 60.0; return; }

        if (id == 10) { obj.isPortal = true; obj.gravityType = 1; obj.height = 60.0; return; }
        if (id == 11) { obj.isPortal = true; obj.gravityType = 0; obj.height = 60.0; return; }

        if (id == 99 || id == 101) { obj.isPortal = true; obj.height = 60.0; return; }

        if (id == 35)   { obj.isPad = true; obj.padType = 0; obj.width = 30.0; obj.height = 10.0; return; }
        if (id == 140)  { obj.isPad = true; obj.padType = 1; obj.width = 30.0; obj.height = 10.0; return; }
        if (id == 1332) { obj.isPad = true; obj.padType = 2; obj.width = 30.0; obj.height = 10.0; return; }
        if (id == 67)   { obj.isPad = true; obj.padType = 3; obj.width = 30.0; obj.height = 10.0; return; }

        if (id == 36)   { obj.isOrb = true; obj.orbType = 0; return; }
        if (id == 141)  { obj.isOrb = true; obj.orbType = 1; return; }
        if (id == 1333) { obj.isOrb = true; obj.orbType = 2; return; }
        if (id == 84)   { obj.isOrb = true; obj.orbType = 3; return; }
        if (id == 1022) { obj.isOrb = true; obj.orbType = 4; return; }
        if (id == 1330) { obj.isOrb = true; obj.orbType = 5; return; }
        if (id == 1704 || id == 1751) { obj.isOrb = true; obj.orbType = 6; return; }

        if ((id >= 899 && id <= 950) || (id >= 1006 && id <= 1100) ||
            (id >= 1585 && id <= 1620) || (id >= 2067 && id <= 2099)) {
            obj.isTrigger = true;
            return;
        }

        if ((id >= 1 && id <= 7) || (id >= 40 && id <= 58) ||
            id == 14 || id == 15 || id == 16 || id == 17 || id == 18 ||
            (id >= 62 && id <= 87) || (id >= 106 && id <= 142) ||
            (id >= 467 && id <= 503) || (id >= 661 && id <= 700)) {
            obj.isSolid = true;
            return;
        }
    }

    // ─── PathfinderPanel ───────────────────────────────────────────────

    PathfinderPanel* PathfinderPanel::create(GJGameLevel* level) {
        auto ret = new PathfinderPanel();
        if (ret && ret->init(level)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool PathfinderPanel::init(GJGameLevel* level) {
        if (!CCNode::init()) return false;

        m_level = level;
        m_engine = std::make_unique<PathfinderEngine>();

        PathfinderConfig config;
        config.tps = Mod::get()->getSettingValue<int64_t>("tps");
        config.maxAttempts = Mod::get()->getSettingValue<int64_t>("max-attempts");
        config.lookAheadFrames = Mod::get()->getSettingValue<int64_t>("look-ahead-frames");
        m_engine->setConfig(config);

        this->setContentSize({280.0f, 140.0f});
        this->setAnchorPoint({0.5f, 0.5f});

        m_bgPanel = CCScale9Sprite::create("square02_small.png");
        m_bgPanel->setContentSize({280.0f, 140.0f});
        m_bgPanel->setOpacity(200);
        m_bgPanel->setColor({30, 30, 50});
        m_bgPanel->setAnchorPoint({0.5f, 0.5f});
        m_bgPanel->setPosition({140.0f, 70.0f});
        this->addChild(m_bgPanel);

        auto titleLabel = CCLabelBMFont::create("AI Pathfinder", "bigFont.fnt");
        titleLabel->setScale(0.45f);
        titleLabel->setPosition({140.0f, 125.0f});
        m_bgPanel->addChild(titleLabel);

        auto separator = CCSprite::createWithSpriteFrameName("edit_vLine_001.png");
        if (separator) {
            separator->setScaleX(6.0f);
            separator->setScaleY(0.5f);
            separator->setPosition({140.0f, 115.0f});
            separator->setOpacity(100);
            m_bgPanel->addChild(separator);
        }

        m_menu = CCMenu::create();
        m_menu->setPosition({0, 0});
        m_bgPanel->addChild(m_menu);

        auto runBtnSpr = ButtonSprite::create("Run Pathfinder", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        runBtnSpr->setScale(0.7f);
        m_runButton = CCMenuItemSpriteExtra::create(
            runBtnSpr, this,
            menu_selector(PathfinderPanel::onRunPathfinder)
        );
        m_runButton->setPosition({100.0f, 88.0f});
        m_menu->addChild(m_runButton);

        auto cancelBtnSpr = ButtonSprite::create("Cancel", "goldFont.fnt", "GJ_button_06.png", 0.8f);
        cancelBtnSpr->setScale(0.6f);
        m_cancelButton = CCMenuItemSpriteExtra::create(
            cancelBtnSpr, this,
            menu_selector(PathfinderPanel::onCancelPathfinder)
        );
        m_cancelButton->setPosition({200.0f, 88.0f});
        m_cancelButton->setVisible(false);
        m_menu->addChild(m_cancelButton);

        auto saveBtnSpr = ButtonSprite::create("Save .gdr2", "goldFont.fnt", "GJ_button_02.png", 0.8f);
        saveBtnSpr->setScale(0.7f);
        m_saveButton = CCMenuItemSpriteExtra::create(
            saveBtnSpr, this,
            menu_selector(PathfinderPanel::onSaveGDR2)
        );
        m_saveButton->setPosition({140.0f, 20.0f});
        m_saveButton->setVisible(false);
        m_menu->addChild(m_saveButton);

        m_statusLabel = CCLabelBMFont::create("Ready", "chatFont.fnt");
        m_statusLabel->setScale(0.55f);
        m_statusLabel->setPosition({140.0f, 65.0f});
        m_statusLabel->setColor({200, 200, 200});
        m_bgPanel->addChild(m_statusLabel);

        m_progressLabel = CCLabelBMFont::create("", "chatFont.fnt");
        m_progressLabel->setScale(0.5f);
        m_progressLabel->setPosition({140.0f, 48.0f});
        m_progressLabel->setColor({150, 255, 150});
        m_bgPanel->addChild(m_progressLabel);

        m_progressBarBg = CCSprite::create("square02_small.png");
        if (m_progressBarBg) {
            m_progressBarBg->setTextureRect({0, 0, 1, 1});
            m_progressBarBg->setScaleX(220.0f);
            m_progressBarBg->setScaleY(8.0f);
            m_progressBarBg->setColor({60, 60, 80});
            m_progressBarBg->setPosition({140.0f, 36.0f});
            m_progressBarBg->setVisible(false);
            m_bgPanel->addChild(m_progressBarBg);
        }

        m_progressBarFill = CCSprite::create("square02_small.png");
        if (m_progressBarFill) {
            m_progressBarFill->setTextureRect({0, 0, 1, 1});
            m_progressBarFill->setScaleX(0.0f);
            m_progressBarFill->setScaleY(6.0f);
            m_progressBarFill->setColor({80, 220, 80});
            m_progressBarFill->setAnchorPoint({0.0f, 0.5f});
            m_progressBarFill->setPosition({30.0f, 36.0f});
            m_progressBarFill->setVisible(false);
            m_bgPanel->addChild(m_progressBarFill);
        }

        this->scheduleUpdate();

        return true;
    }

    void PathfinderPanel::extractLevelObjects() {
        m_cachedObjects.clear();

        if (!m_level) return;

        std::string levelString = m_level->m_levelString;
        if (levelString.empty()) {
            log::warn("Pathfinder: Level string is empty");
            return;
        }

        size_t headerEnd = levelString.find(';');
        if (headerEnd == std::string::npos) return;

        std::string header = levelString.substr(0, headerEnd);

        m_cachedStartMode = GameMode::Cube;
        m_cachedStartSpeed = SpeedSetting::Normal;
        m_cachedStartGravity = GravityDirection::Down;
        m_cachedStartMini = false;

        {
            std::istringstream headerStream(header);
            std::string token;
            std::string key;
            bool isKey = true;
            while (std::getline(headerStream, token, ',')) {
                if (isKey) {
                    key = token;
                } else {
                    try {
                        if (key == "kA2") {
                            int gm = std::stoi(token);
                            if (gm >= 0 && gm <= 7)
                                m_cachedStartMode = static_cast<GameMode>(gm);
                        } else if (key == "kA4") {
                            int sp = std::stoi(token);
                            if (sp >= 0 && sp <= 5)
                                m_cachedStartSpeed = static_cast<SpeedSetting>(sp);
                        } else if (key == "kA11") {
                            m_cachedStartGravity = (token == "1")
                                ? GravityDirection::Up : GravityDirection::Down;
                        } else if (key == "kA3") {
                            m_cachedStartMini = (token == "1");
                        }
                    } catch (...) {}
                }
                isKey = !isKey;
            }
        }

        double maxX = 0.0;
        std::string remaining = levelString.substr(headerEnd + 1);
        std::istringstream objStream(remaining);
        std::string objStr;

        while (std::getline(objStream, objStr, ';')) {
            if (objStr.empty()) continue;

            SimObjectData simObj{};
            simObj.isActive = true;
            simObj.width = 30.0;
            simObj.height = 30.0;

            std::istringstream kvStream(objStr);
            std::string kvToken;
            std::string key;
            bool isKey = true;

            while (std::getline(kvStream, kvToken, ',')) {
                if (isKey) {
                    key = kvToken;
                } else {
                    try {
                        if (key == "1") {
                            simObj.objectID = std::stoi(kvToken);
                        } else if (key == "2") {
                            simObj.posX = std::stod(kvToken);
                            maxX = std::max(maxX, simObj.posX);
                        } else if (key == "3") {
                            simObj.posY = std::stod(kvToken);
                        } else if (key == "6") {
                            simObj.rotation = std::stod(kvToken);
                        } else if (key == "128") {
                            double scale = std::stod(kvToken);
                            simObj.width *= scale;
                            simObj.height *= scale;
                        } else if (key == "57") {
                            simObj.groupID = std::stoi(kvToken);
                        }
                    } catch (...) {}
                }
                isKey = !isKey;
            }

            if (simObj.objectID > 0) {
                classifyObject(simObj);
                m_cachedObjects.push_back(simObj);
            }
        }

        m_cachedLevelLength = maxX + 300.0;

        log::info("Pathfinder: Extracted {} objects, level length: {:.0f}",
                  m_cachedObjects.size(), m_cachedLevelLength);
    }

    void PathfinderPanel::onRunPathfinder(CCObject* sender) {
        if (m_isRunning) return;

        m_statusLabel->setString("Extracting level data...");
        m_progressLabel->setString("");

        extractLevelObjects();

        if (m_cachedObjects.empty()) {
            showNotification("Failed to parse level data!", true);
            m_statusLabel->setString("Error: No objects found");
            return;
        }

        m_engine->setLevelData(
            m_cachedObjects, m_cachedLevelLength,
            m_cachedStartMode, m_cachedStartSpeed,
            m_cachedStartGravity, m_cachedStartMini
        );

        m_engine->setProgressCallback([this](double progress) {
            m_lastProgress = progress;
        });

        m_engine->setCompletionCallback([this](bool success) {
            m_hasResult = success;
        });

        m_engine->start();
        m_isRunning = true;

        m_runButton->setVisible(false);
        m_cancelButton->setVisible(true);
        m_saveButton->setVisible(false);
        if (m_progressBarBg) m_progressBarBg->setVisible(true);
        if (m_progressBarFill) m_progressBarFill->setVisible(true);
        m_statusLabel->setString("Analyzing...");
    }

    void PathfinderPanel::onCancelPathfinder(CCObject* sender) {
        if (!m_isRunning) return;

        m_engine->cancel();
        m_isRunning = false;

        m_runButton->setVisible(true);
        m_cancelButton->setVisible(false);
        if (m_progressBarBg) m_progressBarBg->setVisible(false);
        if (m_progressBarFill) m_progressBarFill->setVisible(false);
        m_statusLabel->setString("Cancelled");
        m_progressLabel->setString("");
    }

    void PathfinderPanel::onSaveGDR2(CCObject* sender) {
        if (!m_engine || !m_engine->hasResult()) return;

        auto result = m_engine->getResult();

        GDR2Exporter::GDR2Replay replay;
        replay.author = "AI Pathfinder";
        replay.description = "Auto-generated path";
        replay.fps = Mod::get()->getSettingValue<int64_t>("tps");
        replay.levelID = m_level->m_levelID;
        replay.levelName = m_level->m_levelName;
        replay.frames = result;

        if (!result.empty()) {
            replay.duration = static_cast<double>(result.back().frame) / static_cast<double>(replay.fps);
        }

        auto savePath = Mod::get()->getSaveDir();
        auto filename = GDR2Exporter::getDefaultFilename(replay.levelName, replay.levelID);
        auto fullPath = savePath / filename;

        if (GDR2Exporter::saveToFile(replay, fullPath.string())) {
            std::string msg = "Saved: " + filename;
            showNotification(msg);
            m_statusLabel->setString(msg.c_str());
            log::info("Pathfinder: Saved replay to {}", fullPath.string());
        } else {
            showNotification("Failed to save replay!", true);
            m_statusLabel->setString("Save failed!");
            log::error("Pathfinder: Failed to save replay to {}", fullPath.string());
        }
    }

    void PathfinderPanel::update(float dt) {
        if (!m_isRunning || !m_engine) return;

        auto status = m_engine->getStatus();
        double progress = m_engine->getProgress();

        if (m_progressBarFill) {
            float fillScale = static_cast<float>(progress / 100.0) * 220.0f;
            m_progressBarFill->setScaleX(std::max(0.1f, fillScale));

            if (progress < 30.0) {
                m_progressBarFill->setColor({220, 80, 80});
            } else if (progress < 70.0) {
                m_progressBarFill->setColor({220, 200, 80});
            } else {
                m_progressBarFill->setColor({80, 220, 80});
            }
        }

        char progressText[64];
        snprintf(progressText, sizeof(progressText), "Analysis: %.1f%% completed", progress);
        m_progressLabel->setString(progressText);

        if (status != PathfinderStatus::Running) {
            m_isRunning = false;

            m_runButton->setVisible(true);
            m_cancelButton->setVisible(false);

            if (status == PathfinderStatus::Success) {
                m_statusLabel->setString("Path Found!");
                m_statusLabel->setColor({80, 255, 80});
                m_saveButton->setVisible(true);
                m_progressLabel->setString("Analysis: 100% completed");

                if (m_progressBarFill) {
                    m_progressBarFill->setScaleX(220.0f);
                    m_progressBarFill->setColor({80, 255, 80});
                }

                m_saveButton->setScale(0.0f);
                m_saveButton->runAction(CCEaseElasticOut::create(
                    CCScaleTo::create(0.5f, 0.7f), 0.6f
                ));

                showNotification("Path found! Click 'Save .gdr2' to export.");
            } else if (status == PathfinderStatus::Failed) {
                m_statusLabel->setString("No Path Found");
                m_statusLabel->setColor({255, 80, 80});
                if (m_progressBarFill) {
                    m_progressBarFill->setColor({220, 80, 80});
                }
                showNotification("Pathfinder could not find a safe path.", true);
            } else if (status == PathfinderStatus::Cancelled) {
                m_statusLabel->setString("Cancelled");
                m_statusLabel->setColor({200, 200, 200});
            }
        }
    }

    void PathfinderPanel::cleanup() {
        if (m_engine) {
            m_engine->cancel();
        }
        this->unscheduleUpdate();
        CCNode::cleanup();
    }

    void PathfinderPanel::showNotification(const std::string& text, bool isError) {
        if (isError) {
            Notification::create(text, NotificationIcon::Error, 3.0f)->show();
        } else {
            Notification::create(text, NotificationIcon::Success, 3.0f)->show();
        }
    }

    void PathfinderPanel::updateUI() {
        if (m_engine) {
            PathfinderConfig config;
            config.tps = Mod::get()->getSettingValue<int64_t>("tps");
            config.maxAttempts = Mod::get()->getSettingValue<int64_t>("max-attempts");
            config.lookAheadFrames = Mod::get()->getSettingValue<int64_t>("look-ahead-frames");
            m_engine->setConfig(config);
        }
    }

} // namespace pathfinder