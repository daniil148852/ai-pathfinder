#include "PhysicsSimulator.hpp"
#include <algorithm>
#include <cmath>

namespace pathfinder {

    PhysicsSimulator::PhysicsSimulator() {
        reset();
    }

    PhysicsSimulator::~PhysicsSimulator() = default;

    void PhysicsSimulator::setObjects(std::vector<SimObjectData> objects) {
        m_objects = std::move(objects);
        buildSpatialHash();
    }

    void PhysicsSimulator::setLevelLength(double length) {
        m_levelLength = length;
    }

    void PhysicsSimulator::setStartGameMode(GameMode mode) {
        m_state.gameMode = mode;
    }

    void PhysicsSimulator::setStartSpeed(SpeedSetting speed) {
        m_state.speed = speed;
    }

    void PhysicsSimulator::setStartGravity(GravityDirection grav) {
        m_state.gravity = grav;
    }

    void PhysicsSimulator::setStartMini(bool mini) {
        m_state.playerSize = mini ? PlayerSize::Mini : PlayerSize::Normal;
    }

    void PhysicsSimulator::setTPS(int tps) {
        m_tps = tps;
        m_dt = 1.0 / static_cast<double>(tps);
    }

    void PhysicsSimulator::reset() {
        m_state = PlayerState();
        m_state.posX = 0.0;
        m_state.posY = Physics::GROUND_Y + 15.0; // Half player height above ground
        m_state.velocityY = 0.0;
        m_state.isOnGround = true;
        m_state.isDead = false;
        m_state.currentFrame = 0;
        m_frameData.clear();
    }

    PlayerState& PhysicsSimulator::getState() {
        return m_state;
    }

    const PlayerState& PhysicsSimulator::getState() const {
        return m_state;
    }

    void PhysicsSimulator::buildSpatialHash() {
        m_spatialHash.clear();
        for (int i = 0; i < static_cast<int>(m_objects.size()); i++) {
            const auto& obj = m_objects[i];
            auto hb = obj.getHitbox();

            int minCX = static_cast<int>(std::floor(hb.x / CELL_SIZE));
            int maxCX = static_cast<int>(std::floor((hb.x + hb.width) / CELL_SIZE));
            int minCY = static_cast<int>(std::floor(hb.y / CELL_SIZE));
            int maxCY = static_cast<int>(std::floor((hb.y + hb.height) / CELL_SIZE));

            for (int cx = minCX; cx <= maxCX; cx++) {
                for (int cy = minCY; cy <= maxCY; cy++) {
                    m_spatialHash[hashCell(cx, cy)].push_back(i);
                }
            }
        }
    }

    std::vector<int> PhysicsSimulator::getObjectsNearPlayer() const {
        auto phb = m_state.getHitbox();
        // Expand search area slightly for upcoming objects
        double lookAhead = Physics::getSpeedMultiplier(m_state.speed) * 30.0;

        int minCX = static_cast<int>(std::floor((phb.x - 30.0) / CELL_SIZE));
        int maxCX = static_cast<int>(std::floor((phb.x + phb.width + lookAhead) / CELL_SIZE));
        int minCY = static_cast<int>(std::floor((phb.y - 60.0) / CELL_SIZE));
        int maxCY = static_cast<int>(std::floor((phb.y + phb.height + 60.0) / CELL_SIZE));

        std::unordered_set<int> seen;
        std::vector<int> result;

        for (int cx = minCX; cx <= maxCX; cx++) {
            for (int cy = minCY; cy <= maxCY; cy++) {
                auto it = m_spatialHash.find(hashCell(cx, cy));
                if (it != m_spatialHash.end()) {
                    for (int idx : it->second) {
                        if (seen.insert(idx).second) {
                            result.push_back(idx);
                        }
                    }
                }
            }
        }
        return result;
    }

    double PhysicsSimulator::getGravitySign() const {
        return (m_state.gravity == GravityDirection::Down) ? 1.0 : -1.0;
    }

    double PhysicsSimulator::getGravityForCurrentMode() const {
        bool mini = (m_state.playerSize == PlayerSize::Mini);
        double gSign = getGravitySign();

        switch (m_state.gameMode) {
            case GameMode::Cube:
            case GameMode::Ball:
            case GameMode::Robot:
            case GameMode::Spider:
            case GameMode::UFO:
                return (mini ? Physics::GRAVITY_MINI : Physics::GRAVITY_NORMAL) * gSign;
            case GameMode::Ship:
                return 0.0; // Ship handles gravity differently
            case GameMode::Wave:
                return 0.0; // Wave has no gravity
            case GameMode::Swing:
                return Physics::SWING_GRAVITY * gSign;
            default:
                return Physics::GRAVITY_NORMAL * gSign;
        }
    }

    double PhysicsSimulator::getJumpVelocity() const {
        bool mini = (m_state.playerSize == PlayerSize::Mini);
        double gSign = getGravitySign();

        switch (m_state.gameMode) {
            case GameMode::Cube:
                return (mini ? Physics::CUBE_JUMP_VELOCITY_MINI : Physics::CUBE_JUMP_VELOCITY) * gSign;
            case GameMode::Ball:
                return (mini ? Physics::BALL_SWITCH_VELOCITY_MINI : Physics::BALL_SWITCH_VELOCITY) * gSign;
            case GameMode::UFO:
                return (mini ? Physics::UFO_CLICK_VELOCITY_MINI : Physics::UFO_CLICK_VELOCITY) * gSign;
            case GameMode::Robot:
                return (mini ? Physics::ROBOT_JUMP_VELOCITY_MINI : Physics::ROBOT_JUMP_VELOCITY) * gSign;
            case GameMode::Swing:
                return Physics::SWING_CLICK_VELOCITY * gSign;
            default:
                return Physics::CUBE_JUMP_VELOCITY * gSign;
        }
    }

    void PhysicsSimulator::step(bool holding) {
        if (m_state.isDead || hasReachedEnd()) return;

        m_state.wasHolding = m_state.isHolding;
        m_state.isHolding = holding;

        // Move player forward
        movePlayer();

        // Apply gamemode-specific physics
        applyGameModePhysics(holding);

        // Check portal interactions
        checkPortals();

        // Check pad/orb interactions
        checkPadsAndOrbs(holding);

        // Check collisions with hazards and solids
        checkCollisions();

        // Clamp position to level bounds
        clampPosition();

        // Update rotation
        updateRotation();

        // Record frame data
        m_frameData.push_back({
            m_state.currentFrame,
            m_state.posX,
            m_state.posY,
            m_state.rotation,
            holding
        });

        m_state.currentFrame++;
        m_state.percentComplete = getProgress();
    }

    void PhysicsSimulator::movePlayer() {
        double xSpeed = Physics::getSpeedMultiplier(m_state.speed);
        m_state.posX += xSpeed;
    }

    void PhysicsSimulator::applyGameModePhysics(bool holding) {
        switch (m_state.gameMode) {
            case GameMode::Cube:   applyCubePhysics(holding);   break;
            case GameMode::Ship:   applyShipPhysics(holding);   break;
            case GameMode::Ball:   applyBallPhysics(holding);   break;
            case GameMode::UFO:    applyUFOPhysics(holding);    break;
            case GameMode::Wave:   applyWavePhysics(holding);   break;
            case GameMode::Robot:  applyRobotPhysics(holding);  break;
            case GameMode::Spider: applySpiderPhysics(holding); break;
            case GameMode::Swing:  applySwingPhysics(holding);  break;
        }
    }

    void PhysicsSimulator::applyCubePhysics(bool holding) {
        double grav = getGravityForCurrentMode();
        double gSign = getGravitySign();

        // Jump on press (not hold) while on ground
        if (holding && !m_state.wasHolding && m_state.isOnGround) {
            m_state.velocityY = getJumpVelocity();
            m_state.isOnGround = false;
        }

        // Apply gravity
        if (!m_state.isOnGround) {
            m_state.velocityY += grav;

            // Clamp fall speed
            double maxFall = (m_state.playerSize == PlayerSize::Mini)
                ? Physics::MAX_FALL_SPEED_MINI : Physics::MAX_FALL_SPEED;
            if (gSign > 0) {
                m_state.velocityY = std::max(m_state.velocityY, maxFall);
            } else {
                m_state.velocityY = std::min(m_state.velocityY, -maxFall);
            }
        }

        m_state.posY += m_state.velocityY;
    }

    void PhysicsSimulator::applyShipPhysics(bool holding) {
        bool mini = (m_state.playerSize == PlayerSize::Mini);
        double gSign = getGravitySign();

        if (holding) {
            double upGrav = mini ? Physics::SHIP_GRAVITY_UP_MINI : Physics::SHIP_GRAVITY_UP;
            m_state.velocityY += upGrav * gSign;
            double maxUp = Physics::SHIP_MAX_UP * gSign;
            if (gSign > 0) {
                m_state.velocityY = std::min(m_state.velocityY, std::abs(maxUp));
            } else {
                m_state.velocityY = std::max(m_state.velocityY, -std::abs(maxUp));
            }
        } else {
            double downGrav = mini ? Physics::SHIP_GRAVITY_DOWN_MINI : Physics::SHIP_GRAVITY_DOWN;
            m_state.velocityY += downGrav * gSign;
            double maxDown = Physics::SHIP_MAX_DOWN * gSign;
            if (gSign > 0) {
                m_state.velocityY = std::max(m_state.velocityY, maxDown);
            } else {
                m_state.velocityY = std::min(m_state.velocityY, -maxDown);
            }
        }

        m_state.posY += m_state.velocityY;
        m_state.isOnGround = false;
    }

    void PhysicsSimulator::applyBallPhysics(bool holding) {
        double grav = getGravityForCurrentMode();
        double gSign = getGravitySign();

        // Ball: click to switch gravity direction
        if (holding && !m_state.wasHolding && m_state.isOnGround) {
            m_state.gravity = (m_state.gravity == GravityDirection::Down)
                ? GravityDirection::Up : GravityDirection::Down;
            m_state.velocityY = getJumpVelocity();
            m_state.isOnGround = false;
        }

        // Apply gravity with new direction
        grav = getGravityForCurrentMode();
        gSign = getGravitySign();

        if (!m_state.isOnGround) {
            m_state.velocityY += grav;
            double maxFall = (m_state.playerSize == PlayerSize::Mini)
                ? Physics::MAX_FALL_SPEED_MINI : Physics::MAX_FALL_SPEED;
            if (gSign > 0) {
                m_state.velocityY = std::max(m_state.velocityY, maxFall);
            } else {
                m_state.velocityY = std::min(m_state.velocityY, -maxFall);
            }
        }

        m_state.posY += m_state.velocityY;
    }

    void PhysicsSimulator::applyUFOPhysics(bool holding) {
        double grav = getGravityForCurrentMode();
        double gSign = getGravitySign();

        // UFO: each click gives a small upward boost
        if (holding && !m_state.wasHolding) {
            m_state.velocityY = getJumpVelocity();
        }

        // Always apply gravity
        m_state.velocityY += grav;

        double maxFall = (m_state.playerSize == PlayerSize::Mini)
            ? Physics::MAX_FALL_SPEED_MINI : Physics::MAX_FALL_SPEED;
        if (gSign > 0) {
            m_state.velocityY = std::max(m_state.velocityY, maxFall);
        } else {
            m_state.velocityY = std::min(m_state.velocityY, -maxFall);
        }

        m_state.posY += m_state.velocityY;
    }

    void PhysicsSimulator::applyWavePhysics(bool holding) {
        double gSign = getGravitySign();
        double xSpeed = Physics::getSpeedMultiplier(m_state.speed);

        // Wave: holding = go up at 45 degrees, not holding = go down at 45 degrees
        if (holding) {
            m_state.velocityY = xSpeed * Physics::WAVE_SLOPE_UP * gSign;
        } else {
            m_state.velocityY = xSpeed * Physics::WAVE_SLOPE_DOWN * gSign;
        }

        m_state.posY += m_state.velocityY;
        m_state.isOnGround = false;
    }

    void PhysicsSimulator::applyRobotPhysics(bool holding) {
        double grav = getGravityForCurrentMode();
        double gSign = getGravitySign();

        // Robot: click to jump, hold to jump higher (variable jump height)
        if (holding && !m_state.wasHolding && m_state.isOnGround) {
            m_state.velocityY = getJumpVelocity();
            m_state.isOnGround = false;
            m_state.robotJumping = true;
            m_state.robotJumpTime = 0.0;
        }

        if (m_state.robotJumping && holding) {
            m_state.robotJumpTime += m_dt;
            if (m_state.robotJumpTime < PlayerState::ROBOT_MAX_JUMP_TIME) {
                // Reduced gravity while holding (extends jump)
                m_state.velocityY += Physics::ROBOT_GRAVITY_HOLD * gSign;
            } else {
                m_state.robotJumping = false;
            }
        }

        if (!holding) {
            m_state.robotJumping = false;
        }

        // Apply gravity
        if (!m_state.isOnGround) {
            m_state.velocityY += grav;

            double maxFall = (m_state.playerSize == PlayerSize::Mini)
                ? Physics::MAX_FALL_SPEED_MINI : Physics::MAX_FALL_SPEED;
            if (gSign > 0) {
                m_state.velocityY = std::max(m_state.velocityY, maxFall);
            } else {
                m_state.velocityY = std::min(m_state.velocityY, -maxFall);
            }
        }

        m_state.posY += m_state.velocityY;
    }

    void PhysicsSimulator::applySpiderPhysics(bool holding) {
        double grav = getGravityForCurrentMode();
        double gSign = getGravitySign();

        // Spider: click to teleport to opposite surface
        if (holding && !m_state.wasHolding && m_state.isOnGround) {
            // Flip gravity
            m_state.gravity = (m_state.gravity == GravityDirection::Down)
                ? GravityDirection::Up : GravityDirection::Down;

            // Teleport: find nearest solid surface in new gravity direction
            double newGSign = getGravitySign();
            double scanDir = -newGSign; // Scan toward the surface in new gravity

            // Simple teleport: scan for nearest solid
            double targetY = m_state.posY;
            bool foundSurface = false;

            auto nearObjects = getObjectsNearPlayer();
            double bestDist = 999999.0;

            for (int idx : nearObjects) {
                const auto& obj = m_objects[idx];
                if (!obj.isSolid || !obj.isActive) continue;

                auto ohb = obj.getHitbox();
                // Check X overlap
                auto phb = m_state.getHitbox();
                if (phb.x + phb.width < ohb.x || ohb.x + ohb.width < phb.x) continue;

                double surfaceY;
                if (scanDir > 0) {
                    // Looking upward
                    surfaceY = ohb.y; // Bottom of solid
                    if (surfaceY > m_state.posY) {
                        double dist = surfaceY - m_state.posY;
                        if (dist < bestDist) {
                            bestDist = dist;
                            double halfH = phb.height * 0.5;
                            targetY = surfaceY - halfH;
                            foundSurface = true;
                        }
                    }
                } else {
                    // Looking downward
                    surfaceY = ohb.y + ohb.height; // Top of solid
                    if (surfaceY < m_state.posY) {
                        double dist = m_state.posY - surfaceY;
                        if (dist < bestDist) {
                            bestDist = dist;
                            double halfH = phb.height * 0.5;
                            targetY = surfaceY + halfH;
                            foundSurface = true;
                        }
                    }
                }
            }

            // If no solid found, teleport to ground/ceiling
            if (!foundSurface) {
                double halfH = m_state.getHitbox().height * 0.5;
                if (m_state.gravity == GravityDirection::Down) {
                    targetY = Physics::GROUND_Y + halfH;
                } else {
                    targetY = Physics::CEILING_Y - halfH;
                }
            }

            m_state.posY = targetY;
            m_state.velocityY = Physics::SPIDER_TELEPORT_BOOST * getGravitySign();
            m_state.isOnGround = false;
        }

        // Apply gravity
        if (!m_state.isOnGround) {
            grav = getGravityForCurrentMode();
            m_state.velocityY += grav;

            double maxFall = (m_state.playerSize == PlayerSize::Mini)
                ? Physics::MAX_FALL_SPEED_MINI : Physics::MAX_FALL_SPEED;
            gSign = getGravitySign();
            if (gSign > 0) {
                m_state.velocityY = std::max(m_state.velocityY, maxFall);
            } else {
                m_state.velocityY = std::min(m_state.velocityY, -maxFall);
            }
        }

        m_state.posY += m_state.velocityY;
    }

    void PhysicsSimulator::applySwingPhysics(bool holding) {
        double gSign = getGravitySign();

        // Swing: click toggles the swing direction
        if (holding && !m_state.wasHolding) {
            m_state.velocityY = Physics::SWING_CLICK_VELOCITY * gSign *
                                static_cast<double>(m_state.swingDirection);
            m_state.swingDirection *= -1;
        }

        // Apply swing gravity (always pulls toward center/ground)
        m_state.velocityY += Physics::SWING_GRAVITY * gSign;

        // Clamp
        if (std::abs(m_state.velocityY) > Physics::SWING_MAX_SPEED) {
            m_state.velocityY = (m_state.velocityY > 0 ? 1.0 : -1.0) * Physics::SWING_MAX_SPEED;
        }

        m_state.posY += m_state.velocityY;
        m_state.isOnGround = false;
    }

    void PhysicsSimulator::checkCollisions() {
        auto nearObjects = getObjectsNearPlayer();
        auto phb = m_state.getHitbox();

        m_state.isOnGround = false;

        for (int idx : nearObjects) {
            const auto& obj = m_objects[idx];
            if (!obj.isActive) continue;

            if (obj.isSlope) {
                auto ohb = obj.getHitbox();
                if (phb.intersects(ohb)) {
                    handleSlopePhysics(obj);
                }
                continue;
            }

            // Check hazards
            if (obj.isHazard) {
                auto ohb = obj.getInnerHitbox();
                if (phb.intersects(ohb)) {
                    m_state.isDead = true;
                    return;
                }
            }

            // Check solids (platforms, blocks)
            if (obj.isSolid) {
                auto ohb = obj.getHitbox();
                if (phb.intersects(ohb)) {
                    // Determine collision direction
                    double overlapLeft = (phb.x + phb.width) - ohb.x;
                    double overlapRight = (ohb.x + ohb.width) - phb.x;
                    double overlapBottom = (phb.y + phb.height) - ohb.y;
                    double overlapTop = (ohb.y + ohb.height) - phb.y;

                    double minOverlapX = std::min(overlapLeft, overlapRight);
                    double minOverlapY = std::min(overlapBottom, overlapTop);

                    if (minOverlapY < minOverlapX) {
                        // Vertical collision
                        double gSign = getGravitySign();
                        if (gSign > 0) {
                            // Normal gravity
                            if (m_state.velocityY <= 0 && overlapTop < overlapBottom) {
                                // Landing on top
                                m_state.posY = ohb.y + ohb.height + phb.height * 0.5;
                                m_state.velocityY = 0;
                                m_state.isOnGround = true;
                            } else if (m_state.velocityY > 0 && overlapBottom < overlapTop) {
                                // Hitting ceiling
                                m_state.posY = ohb.y - phb.height * 0.5;
                                m_state.velocityY = 0;
                            }
                        } else {
                            // Flipped gravity
                            if (m_state.velocityY >= 0 && overlapBottom < overlapTop) {
                                m_state.posY = ohb.y - phb.height * 0.5;
                                m_state.velocityY = 0;
                                m_state.isOnGround = true;
                            } else if (m_state.velocityY < 0 && overlapTop < overlapBottom) {
                                m_state.posY = ohb.y + ohb.height + phb.height * 0.5;
                                m_state.velocityY = 0;
                            }
                        }
                    } else {
                        // Horizontal collision - death in most cases for cube/ball/wave
                        if (m_state.gameMode != GameMode::Ship &&
                            m_state.gameMode != GameMode::Swing) {
                            // Side collision = death
                            if (overlapLeft < overlapRight) {
                                m_state.isDead = true;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }

    void PhysicsSimulator::handleSlopePhysics(const SimObjectData& slope) {
        auto phb = m_state.getHitbox();
        auto shb = slope.getHitbox();

        // Calculate slope surface Y at player's X position
        double relX = (m_state.posX - shb.x) / shb.width;
        relX = std::clamp(relX, 0.0, 1.0);

        double slopeAngleRad = slope.slopeAngle * M_PI / 180.0;
        double slopeHeight = std::tan(slopeAngleRad) * shb.width;

        double surfaceY;
        if (slopeAngleRad > 0) {
            // Ascending slope
            surfaceY = shb.y + relX * slopeHeight;
        } else {
            // Descending slope
            surfaceY = shb.y + shb.height + relX * slopeHeight;
        }

        double halfH = phb.height * 0.5;
        double gSign = getGravitySign();

        if (gSign > 0 && m_state.posY - halfH < surfaceY && m_state.velocityY <= 0) {
            m_state.posY = surfaceY + halfH;
            m_state.velocityY = 0;
            m_state.isOnGround = true;
        } else if (gSign < 0 && m_state.posY + halfH > surfaceY && m_state.velocityY >= 0) {
            m_state.posY = surfaceY - halfH;
            m_state.velocityY = 0;
            m_state.isOnGround = true;
        }
    }

    void PhysicsSimulator::checkPortals() {
        auto nearObjects = getObjectsNearPlayer();
        auto phb = m_state.getHitbox();

        for (int idx : nearObjects) {
            const auto& obj = m_objects[idx];
            if (!obj.isPortal || !obj.isActive) continue;

            auto ohb = obj.getHitbox();
            if (!phb.intersects(ohb)) continue;

            // Gamemode portals
            if (obj.portalType >= 1 && obj.portalType <= 8) {
                m_state.gameMode = static_cast<GameMode>(obj.portalType - 1);
                if (m_state.gameMode == GameMode::Cube || m_state.gameMode == GameMode::Ball ||
                    m_state.gameMode == GameMode::Robot || m_state.gameMode == GameMode::Spider) {
                    // These modes can be on ground
                } else {
                    m_state.isOnGround = false;
                }
            }

            // Speed portals
            if (obj.speedType >= 0 && obj.speedType <= 5) {
                m_state.speed = static_cast<SpeedSetting>(obj.speedType);
            }

            // Gravity portals
            if (obj.gravityType >= 0) {
                GravityDirection newGrav = static_cast<GravityDirection>(obj.gravityType);
                if (newGrav != m_state.gravity) {
                    m_state.gravity = newGrav;
                    // Invert Y velocity on gravity change
                    m_state.velocityY = -m_state.velocityY;
                    m_state.isOnGround = false;
                }
            }

            // Size portals (handled via objectID detection)
            // Mini portal: objectID 101 (example)
            // Normal portal: objectID 99 (example)

            // Warp portal
            if (obj.isWarpPortal) {
                m_state.posX = obj.warpTargetX;
            }
        }
    }

    void PhysicsSimulator::checkPadsAndOrbs(bool holding) {
        auto nearObjects = getObjectsNearPlayer();
        auto phb = m_state.getHitbox();

        for (int idx : nearObjects) {
            const auto& obj = m_objects[idx];
            if (!obj.isActive) continue;

            auto ohb = obj.getHitbox();
            if (!phb.intersects(ohb)) continue;

            // Pads: activate on contact
            if (obj.isPad) {
                double gSign = getGravitySign();
                switch (obj.padType) {
                    case 0: // Yellow pad
                        m_state.velocityY = getJumpVelocity() * 1.0;
                        m_state.isOnGround = false;
                        break;
                    case 1: // Pink pad
                        m_state.velocityY = getJumpVelocity() * 0.7;
                        m_state.isOnGround = false;
                        break;
                    case 2: // Red pad
                        m_state.velocityY = getJumpVelocity() * 1.4;
                        m_state.isOnGround = false;
                        break;
                    case 3: // Blue pad
                        // Gravity flip
                        m_state.gravity = (m_state.gravity == GravityDirection::Down)
                            ? GravityDirection::Up : GravityDirection::Down;
                        m_state.velocityY = getJumpVelocity();
                        m_state.isOnGround = false;
                        break;
                }
            }

            // Orbs: activate on click while touching
            if (obj.isOrb && holding && !m_state.wasHolding) {
                double gSign = getGravitySign();
                switch (obj.orbType) {
                    case 0: // Yellow orb
                        m_state.velocityY = getJumpVelocity() * 1.0;
                        m_state.isOnGround = false;
                        break;
                    case 1: // Pink orb
                        m_state.velocityY = getJumpVelocity() * 0.7;
                        m_state.isOnGround = false;
                        break;
                    case 2: // Red orb
                        m_state.velocityY = getJumpVelocity() * 1.4;
                        m_state.isOnGround = false;
                        break;
                    case 3: // Blue orb
                        m_state.gravity = (m_state.gravity == GravityDirection::Down)
                            ? GravityDirection::Up : GravityDirection::Down;
                        m_state.velocityY = getJumpVelocity();
                        m_state.isOnGround = false;
                        break;
                    case 4: // Green orb
                        m_state.velocityY = -m_state.velocityY;
                        m_state.gravity = (m_state.gravity == GravityDirection::Down)
                            ? GravityDirection::Up : GravityDirection::Down;
                        break;
                    case 5: // Black orb
                        m_state.velocityY = -getJumpVelocity() * 0.9;
                        break;
                    case 6: // Dash orb (2.1+)
                        m_state.isDashing = true;
                        break;
                }
            }
        }
    }

    void PhysicsSimulator::clampPosition() {
        double halfH = m_state.getHitbox().height * 0.5;
        double gSign = getGravitySign();

        // Ground collision
        if (gSign > 0) {
            if (m_state.posY - halfH <= Physics::GROUND_Y) {
                m_state.posY = Physics::GROUND_Y + halfH;
                m_state.velocityY = 0;
                m_state.isOnGround = true;
            }
            if (m_state.posY + halfH >= Physics::CEILING_Y) {
                m_state.posY = Physics::CEILING_Y - halfH;
                m_state.velocityY = 0;
                if (m_state.gameMode == GameMode::Ship || m_state.gameMode == GameMode::Swing) {
                    // Ships can ride ceiling
                } else {
                    // Other modes might die on ceiling or just clamp
                }
            }
        } else {
            // Flipped gravity
            if (m_state.posY + halfH >= Physics::CEILING_Y) {
                m_state.posY = Physics::CEILING_Y - halfH;
                m_state.velocityY = 0;
                m_state.isOnGround = true;
            }
            if (m_state.posY - halfH <= Physics::GROUND_Y) {
                m_state.posY = Physics::GROUND_Y + halfH;
                m_state.velocityY = 0;
            }
        }

        // Death below ground (fell out of level)
        if (m_state.posY < -100.0 || m_state.posY > Physics::CEILING_Y + 200.0) {
            m_state.isDead = true;
        }
    }

    void PhysicsSimulator::updateRotation() {
        switch (m_state.gameMode) {
            case GameMode::Cube:
            case GameMode::Robot:
                if (m_state.isOnGround) {
                    // Snap rotation to nearest 90 degrees
                    m_state.rotation = std::round(m_state.rotation / 90.0) * 90.0;
                    m_state.rotationSpeed = 0;
                } else {
                    m_state.rotationSpeed = (m_state.playerSize == PlayerSize::Mini) ? -7.2 : -5.8;
                    m_state.rotation += m_state.rotationSpeed * getGravitySign();
                }
                break;
            case GameMode::Ball:
                m_state.rotationSpeed = -6.0;
                m_state.rotation += m_state.rotationSpeed * getGravitySign();
                break;
            case GameMode::Ship:
            case GameMode::Swing: {
                // Rotate based on Y velocity
                double targetRot = -m_state.velocityY * 2.5;
                targetRot = std::clamp(targetRot, -30.0, 30.0);
                m_state.rotation += (targetRot - m_state.rotation) * 0.15;
                break;
            }
            case GameMode::Wave: {
                if (m_state.velocityY > 0) {
                    m_state.rotation = -45.0 * getGravitySign();
                } else {
                    m_state.rotation = 45.0 * getGravitySign();
                }
                break;
            }
            case GameMode::UFO:
                if (m_state.isOnGround) {
                    m_state.rotation = 0;
                } else {
                    double targetRot = -m_state.velocityY * 1.5;
                    targetRot = std::clamp(targetRot, -20.0, 20.0);
                    m_state.rotation += (targetRot - m_state.rotation) * 0.1;
                }
                break;
            case GameMode::Spider:
                if (m_state.isOnGround) {
                    m_state.rotation = std::round(m_state.rotation / 90.0) * 90.0;
                } else {
                    m_state.rotationSpeed = -5.8;
                    m_state.rotation += m_state.rotationSpeed * getGravitySign();
                }
                break;
        }
    }

    bool PhysicsSimulator::hasReachedEnd() const {
        return m_state.posX >= m_levelLength;
    }

    double PhysicsSimulator::getProgress() const {
        if (m_levelLength <= 0.0) return 0.0;
        return std::clamp(m_state.posX / m_levelLength * 100.0, 0.0, 100.0);
    }

    const std::vector<FrameData>& PhysicsSimulator::getFrameData() const {
        return m_frameData;
    }

} // namespace pathfinder