#include "PathfinderUI.hpp"
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

namespace pathfinder {

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

        // Configure engine from settings
        PathfinderConfig config;
        config.tps = Mod::get()->getSettingValue<int64_t>("tps");
        config.maxAttempts = Mod::get()->getSettingValue<int64_t>("max-attempts");
        config.lookAheadFrames = Mod::get()->getSettingValue<int64_t>("look-ahead-frames");
        m_engine->setConfig(config);

        // Build background panel
        m_bgPanel = CCScale9Sprite::create("square02_small.png");
        m_bgPanel->setContentSize({280.0f, 140.0f});
        m_bgPanel->setOpacity(200);
        m_bgPanel->setColor({30, 30, 50});
        this->addChild(m_bgPanel);

        // Title
        auto titleLabel = CCLabelBMFont::create("AI Pathfinder", "bigFont.fnt");
        titleLabel->setScale(0.45f);
        titleLabel->setPosition({140.0f, 125.0f});
        m_bgPanel->addChild(titleLabel);

        // Separator line
        auto separator = CCSprite::createWithSpriteFrameName("edit_vLine_001.png");
        separator->setScaleX(6.0f);
        separator->setScaleY(0.5f);
        separator->setPosition({140.0f, 115.0f});
        separator->setOpacity(100);
        m_bgPanel->addChild(separator);

        // Create menu
        m_menu = CCMenu::create();
        m_menu->setPosition({0, 0});
        m_bgPanel->addChild(m_menu);

        // Run Pathfinder button
        auto runBtnSpr = ButtonSprite::create("Run Pathfinder", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        runBtnSpr->setScale(0.7f);
        m_runButton = CCMenuItemSpriteExtra::create(
            runBtnSpr,
            this,
            menu_selector(PathfinderPanel::onRunPathfinder)
        );
        m_runButton->setPosition({100.0f, 88.0f});
        m_menu->addChild(m_runButton);

        // Cancel button (hidden initially)
        auto cancelBtnSpr = ButtonSprite::create("Cancel", "goldFont.fnt", "GJ_button_06.png", 0.8f);
        cancelBtnSpr->setScale(0.6f);
        m_cancelButton = CCMenuItemSpriteExtra::create(
            cancelBtnSpr,
            this,
            menu_selector(PathfinderPanel::onCancelPathfinder)
        );
        m_cancelButton->setPosition({200.0f, 88.0f});
        m_cancelButton->setVisible(false);
        m_menu->addChild(m_cancelButton);

        // Save .gdr2 button (hidden initially)
        auto saveBtnSpr = ButtonSprite::create("Save .gdr2", "goldFont.fnt", "GJ_button_02.png", 0.8f);
        saveBtnSpr->setScale(0.7f);
        m_saveButton = CCMenuItemSpriteExtra::create(
            saveBtnSpr,
            this,
            menu_selector(PathfinderPanel::onSaveGDR2)
        );
        m_saveButton->setPosition({140.0f, 20.0f});
        m_saveButton->setVisible(false);
        m_menu->addChild(m_saveButton);

        // Status label
        m_statusLabel = CCLabelBMFont::create("Ready", "chatFont.fnt");
        m_statusLabel->setScale(0.55f);
        m_statusLabel->setPosition({140.0f, 65.0f});
        m_statusLabel->setColor({200, 200, 200});
        m_bgPanel->addChild(m_statusLabel);

        // Progress label
        m_progressLabel = CCLabelBMFont::create("", "chatFont.fnt");
        m_progressLabel->setScale(0.5f);
        m_progressLabel->setPosition({140.0f, 48.0f});
        m_progressLabel->setColor({150, 255, 150});
        m_bgPanel->addChild(m_progressLabel);

        // Progress bar background
        m_progressBarBg = CCSprite::create("square02_small.png");
        m_progressBarBg->setTextureRect({0, 0, 1, 1});
        m_progressBarBg->setScaleX(220.0f);
        m_progressBarBg->setScaleY(8.0f);
        m_progressBarBg->setColor({60, 60, 80});
        m_progressBarBg->setPosition({140.0f, 36.0f});
        m_progressBarBg->setVisible(false);
        m_bgPanel->addChild(m_progressBarBg);

        // Progress bar fill
        m_progressBarFill = CCSprite::create("square02_small.png");
        m_progressBarFill->setTextureRect({0, 0, 1, 1});
        m_progressBarFill->setScaleX(0.0f);
        m_progressBarFill->setScaleY(6.0f);
        m_progressBarFill->setColor({80, 220, 80});
        m_progressBarFill->setAnchorPoint({0.0f, 0.5f});
        m_progressBarFill->setPosition({30.0f, 36.0f});
        m_progressBarFill->setVisible(false);
        m_bgPanel->addChild(m_progressBarFill);

        // Schedule updates
        this->scheduleUpdate();

        return true;
    }

    void PathfinderPanel::extractLevelObjects() {
        m_cachedObjects.clear();

        if (!m_level) return;

        // Decode the level string
        std::string levelString = m_level->m_levelString;
        if (levelString.empty()) {
            log::warn("Pathfinder: Level string is empty");
            return;
        }

        // Parse level header and objects
        // GD level format: header;obj1;obj2;...
        // Each object: key,value,key,value,...

        // Find header separator
        size_t headerEnd = levelString.find(';');
        if (headerEnd == std::string::npos) return;

        // Parse header for level settings
        std::string header = levelString.substr(0, headerEnd);

        // Default settings
        m_cachedStartMode = GameMode::Cube;
        m_cachedStartSpeed = SpeedSetting::Normal;
        m_cachedStartGravity = GravityDirection::Down;
        m_cachedStartMini = false;

        // Parse header key-value pairs
        {
            std::istringstream headerStream(header);
            std::string token;
            std::string key;
            bool isKey = true;
            while (std::getline(headerStream, token, ',')) {
                if (isKey) {
                    key = token;
                } else {
                    if (key == "kA2") { // Gamemode
                        int gm = std::stoi(token);
                        if (gm >= 0 && gm <= 7)
                            m_cachedStartMode = static_cast<GameMode>(gm);
                    } else if (key == "kA4") { // Speed
                        int sp = std::stoi(token);
                        if (sp >= 0 && sp <= 5)
                            m_cachedStartSpeed = static_cast<SpeedSetting>(sp);
                    } else if (key == "kA11") { // Gravity flipped
                        m_cachedStartGravity = (token == "1")
                            ? GravityDirection::Up : GravityDirection::Down;
                    } else if (key == "kA3") { // Mini mode
                        m_cachedStartMini = (token == "1");
                    }
                }
                isKey = !isKey;
            }
        }

        // Parse objects
        double maxX = 0.0;
        std::string remaining = levelString.substr(headerEnd + 1);
        std::istringstream objStream(remaining);
        std::string objStr;

        while (std::getline(objStream, objStr, ';')) {
            if (objStr.empty()) continue;

            SimObjectData simObj{};
            simObj.isActive = true;

            // Parse object key-value pairs
            std::istringstream kvStream(objStr);
            std::string kvToken;
            std::string key;
            bool isKey = true;

            while (std::getline(kvStream, kvToken, ',')) {
                if (isKey) {
                    key = kvToken;
                } else {
                    try {
                        if (key == "1") { // Object ID
                            simObj.objectID = std::stoi(kvToken);
                        } else if (key == "2") { // X position
                            simObj.posX = std::stod(kvToken);
                            maxX = std::max(maxX, simObj.posX);
                        } else if (key == "3") { // Y position
                            simObj.posY = std::stod(kvToken);
                        } else if (key == "6") { // Rotation
                            simObj.rotation = std::stod(kvToken);
                        } else if (key == "128") { // Scale
                            double scale = std::stod(kvToken);
                            simObj.width *= scale;
                            simObj.height *= scale;
                        } else if (key == "57") { // Group ID
                            simObj.groupID = std::stoi(kvToken);
                        }
                    } catch (...) {
                        // Skip malformed values
                    }
                }
                isKey = !isKey;
            }

            // Classify object by ID
            classifyObject(simObj);

            if (simObj.objectID > 0) {
                m_cachedObjects.push_back(simObj);
            }
        }

        m_cachedLevelLength = maxX + 300.0; // Some padding

        log::info("Pathfinder: Extracted {} objects, level length: {:.0f}",
                  m_cachedObjects.size(), m_cachedLevelLength);
    }

    // Helper to classify GD objects by their ID
    static void classifyObject(SimObjectData& obj) {
        int id = obj.objectID;

        // Default sizes (30x30 block unit)
        obj.width = 30.0;
        obj.height = 30.0;

        // === Hazards (spikes, saws) ===
        // Spikes
        if (id == 8 || id == 39 || id == 103 || id == 392 || id == 421 ||
            id == 422 || id == 9 || id == 61 || id == 243 || id == 244 ||
            id == 363 || id == 364 || id == 365 || id == 366 ||
            id == 446 || id == 447) {
            obj.isHazard = true;
            obj.width = 25.0;
            obj.height = 25.0;
        }
        // Saws
        else if (id == 88 || id == 89 || id == 98 || id == 397 || id == 398 ||
                 id == 399 || id == 1705 || id == 1706 || id == 1707 ||
                 id == 1708 || id == 1709 || id == 1710) {
            obj.isHazard = true;
            obj.width = 40.0;
            obj.height = 40.0;
        }

        // === Solid blocks ===
        else if ((id >= 1 && id <= 7) || (id >= 40 && id <= 58) ||
                 id == 10 || id == 11 || id == 12 || id == 13 || id == 14 ||
                 id == 15 || id == 16 || id == 17 || id == 18 ||
                 (id >= 62 && id <= 87) || (id >= 106 && id <= 142) ||
                 (id >= 467 && id <= 503) || (id >= 660 && id <= 700)) {
            obj.isSolid = true;
        }

        // === Slopes ===
        else if (id == 280 || id == 281 || id == 282 || id == 283 ||
                 id == 462 || id == 463 || id == 464 || id == 465 ||
                 id == 667 || id == 668 || id == 669 || id == 670) {
            obj.isSlope = true;
            obj.isSolid = true;
            // Determine slope angle based on ID
            if (id == 280 || id == 462 || id == 667) {
                obj.slopeAngle = 45.0;
            } else if (id == 281 || id == 463 || id == 668) {
                obj.slopeAngle = -45.0;
            } else if (id == 282 || id == 464 || id == 669) {
                obj.slopeAngle = 26.57; // ~arctan(0.5)
            } else {
                obj.slopeAngle = -26.57;
            }
        }

        // === Portals ===
        // Gamemode portals
        else if (id == 12) { obj.isPortal = true; obj.portalType = 1; obj.height = 60.0; } // Cube
        else if (id == 13) { obj.isPortal = true; obj.portalType = 2; obj.height = 60.0; } // Ship
        else if (id == 47) { obj.isPortal = true; obj.portalType = 3; obj.height = 60.0; } // Ball
        else if (id == 111){ obj.isPortal = true; obj.portalType = 4; obj.height = 60.0; } // UFO
        else if (id == 660){ obj.isPortal = true; obj.portalType = 5; obj.height = 60.0; } // Wave
        // 2.0+ portal IDs
        else if (id == 745){ obj.isPortal = true; obj.portalType = 1; obj.height = 60.0; } // Cube portal (alt)
        else if (id == 746){ obj.isPortal = true; obj.portalType = 2; obj.height = 60.0; } // Ship portal (alt)
        else if (id == 747){ obj.isPortal = true; obj.portalType = 3; obj.height = 60.0; } // Ball portal (alt)
        else if (id == 748){ obj.isPortal = true; obj.portalType = 4; obj.height = 60.0; } // UFO portal (alt)
        else if (id == 749){ obj.isPortal = true; obj.portalType = 5; obj.height = 60.0; } // Wave portal (alt)
        else if (id == 750){ obj.isPortal = true; obj.portalType = 6; obj.height = 60.0; } // Robot portal
        else if (id == 751){ obj.isPortal = true; obj.portalType = 7; obj.height = 60.0; } // Spider portal
        // 2.2 Swing portal
        else if (id == 1331){ obj.isPortal = true; obj.portalType = 8; obj.height = 60.0; } // Swing portal

        // Speed portals
        else if (id == 200){ obj.isPortal = true; obj.speedType = 0; obj.height = 60.0; } // Slow
        else if (id == 201){ obj.isPortal = true; obj.speedType = 1; obj.height = 60.0; } // Normal
        else if (id == 202){ obj.isPortal = true; obj.speedType = 2; obj.height = 60.0; } // Fast
        else if (id == 203){ obj.isPortal = true; obj.speedType = 3; obj.height = 60.0; } // Faster
        else if (id == 1334){obj.isPortal = true; obj.speedType = 4; obj.height = 60.0; } // Fastest
        else if (id == 1587){obj.isPortal = true; obj.speedType = 5; obj.height = 60.0; } // SlowAF (2.2)

        // Gravity portals
        else if (id == 10) { obj.isPortal = true; obj.gravityType = 1; obj.height = 60.0; } // Flip
        else if (id == 11) { obj.isPortal = true; obj.gravityType = 0; obj.height = 60.0; } // Normal

        // Size portals
        else if (id == 99) { obj.isPortal = true; obj.height = 60.0; } // Normal size
        else if (id == 101){ obj.isPortal = true; obj.height = 60.0; } // Mini size

        // === Pads ===
        else if (id == 35) { obj.isPad = true; obj.padType = 0; obj.width = 30.0; obj.height = 10.0; } // Yellow
        else if (id == 140){ obj.isPad = true; obj.padType = 1; obj.width = 30.0; obj.height = 10.0; } // Pink
        else if (id == 1332){obj.isPad = true; obj.padType = 2; obj.width = 30.0; obj.height = 10.0; } // Red
        else if (id == 67) { obj.isPad = true; obj.padType = 3; obj.width = 30.0; obj.height = 10.0; } // Blue

        // === Orbs ===
        else if (id == 36) { obj.isOrb = true; obj.orbType = 0; } // Yellow
        else if (id == 141){ obj.isOrb = true; obj.orbType = 1; } // Pink
        else if (id == 1333){obj.isOrb = true; obj.orbType = 2; } // Red
        else if (id == 84) { obj.isOrb = true; obj.orbType = 3; } // Blue
        else if (id == 1022){obj.isOrb = true; obj.orbType = 4; } // Green
        else if (id == 1330){obj.isOrb = true; obj.orbType = 5; } // Black
        else if (id == 1704){obj.isOrb = true; obj.orbType = 6; } // Dash
        else if (id == 1751){obj.isOrb = true; obj.orbType = 6; } // Dash (2.2 alt)

        // Default: unknown objects treated as non-interactive
        else {
            // Could be decoration, triggers, etc.
            obj.isTrigger = (id >= 899 && id <= 950) || (id >= 1006 && id <= 1100) ||
                            (id >= 1585 && id <= 1620) || (id >= 2067 && id <= 2099);
        }

        // Apply rotation to slope angle
        if (obj.isSlope) {
            obj.slopeAngle += obj.rotation;
        }

        // Set default portal/speed/gravity type to -1 if not a portal
        if (!obj.isPortal) {
            if (obj.speedType == 0 && !obj.isPortal) obj.speedType = -1;
            if (obj.gravityType == 0 && !obj.isPortal) obj.gravityType = -1;
            if (obj.portalType == 0 && !obj.isPortal) obj.portalType = 0;
        }
    }

    void PathfinderPanel::onRunPathfinder(CCObject* sender) {
        if (m_isRunning) return;

        m_statusLabel->setString("Extracting level data...");
        m_progressLabel->setString("");

        // Extract level objects
        extractLevelObjects();

        if (m_cachedObjects.empty()) {
            showNotification("Failed to parse level data!", true);
            m_statusLabel->setString("Error: No objects found");
            return;
        }

        // Set level data
        m_engine->setLevelData(
            m_cachedObjects,
            m_cachedLevelLength,
            m_cachedStartMode,
            m_cachedStartSpeed,
            m_cachedStartGravity,
            m_cachedStartMini
        );

        // Set callbacks
        m_engine->setProgressCallback([this](double progress) {
            // This runs on worker thread - we just store the value
            m_lastProgress = progress;
        });

        m_engine->setCompletionCallback([this](bool success) {
            // This runs on worker thread
            m_hasResult = success;
        });

        // Start
        m_engine->start();
        m_isRunning = true;

        // Update UI state
        m_runButton->setVisible(false);
        m_cancelButton->setVisible(true);
        m_saveButton->setVisible(false);
        m_progressBarBg->setVisible(true);
        m_progressBarFill->setVisible(true);
        m_statusLabel->setString("Analyzing...");
    }

    void PathfinderPanel::onCancelPathfinder(CCObject* sender) {
        if (!m_isRunning) return;

        m_engine->cancel();
        m_isRunning = false;

        m_runButton->setVisible(true);
        m_cancelButton->setVisible(false);
        m_progressBarBg->setVisible(false);
        m_progressBarFill->setVisible(false);
        m_statusLabel->setString("Cancelled");
        m_progressLabel->setString("");
    }

    void PathfinderPanel::onSaveGDR2(CCObject* sender) {
        if (!m_engine->hasResult()) return;

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

        // Save to geode save directory
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
        if (!m_isRunning) return;

        auto status = m_engine->getStatus();
        double progress = m_engine->getProgress();

        // Update progress bar
        float fillScale = static_cast<float>(progress / 100.0) * 220.0f;
        m_progressBarFill->setScaleX(std::max(0.1f, fillScale));

        // Update progress text
        char progressText[64];
        snprintf(progressText, sizeof(progressText), "Analysis: %.1f%% completed", progress);
        m_progressLabel->setString(progressText);

        // Color the bar based on progress
        if (progress < 30.0) {
            m_progressBarFill->setColor({220, 80, 80});
        } else if (progress < 70.0) {
            m_progressBarFill->setColor({220, 200, 80});
        } else {
            m_progressBarFill->setColor({80, 220, 80});
        }

        // Check completion
        if (status != PathfinderStatus::Running) {
            m_isRunning = false;

            m_runButton->setVisible(true);
            m_cancelButton->setVisible(false);

            if (status == PathfinderStatus::Success) {
                m_statusLabel->setString("Path Found!");
                m_statusLabel->setColor({80, 255, 80});
                m_saveButton->setVisible(true);
                m_progressLabel->setString("Analysis: 100% completed");
                m_progressBarFill->setScaleX(220.0f);
                m_progressBarFill->setColor({80, 255, 80});

                // Animate save button appearance
                m_saveButton->setScale(0.0f);
                m_saveButton->runAction(CCEaseElasticOut::create(
                    CCScaleTo::create(0.5f, 0.7f), 0.6f
                ));

                showNotification("Path found! Click 'Save .gdr2' to export.");
            } else if (status == PathfinderStatus::Failed) {
                m_statusLabel->setString("No Path Found");
                m_statusLabel->setColor({255, 80, 80});
                m_progressBarFill->setColor({220, 80, 80});
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
        // Use Geode's notification system
        if (isError) {
            Notification::create(text, NotificationIcon::Error, 3.0f)->show();
        } else {
            Notification::create(text, NotificationIcon::Success, 3.0f)->show();
        }
    }

    void PathfinderPanel::updateUI() {
        // Called when settings change, etc.
        if (m_engine) {
            PathfinderConfig config;
            config.tps = Mod::get()->getSettingValue<int64_t>("tps");
            config.maxAttempts = Mod::get()->getSettingValue<int64_t>("max-attempts");
            config.lookAheadFrames = Mod::get()->getSettingValue<int64_t>("look-ahead-frames");
            m_engine->setConfig(config);
        }
    }

} // namespace pathfinder