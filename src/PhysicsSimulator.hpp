#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include <functional>
#include <unordered_set>
#include <unordered_map>

namespace pathfinder {

    enum class GameMode : int {
        Cube = 0,
        Ship = 1,
        Ball = 2,
        UFO = 3,
        Wave = 4,
        Robot = 5,
        Spider = 6,
        Swing = 7
    };

    enum class PlayerSize : int {
        Normal = 0,
        Mini = 1
    };

    enum class SpeedSetting : int {
        Slow = 0,      // 0.7x
        Normal = 1,    // 1.0x
        Fast = 2,      // 1.2x
        Faster = 3,    // 1.5x
        Fastest = 4,   // 1.8x
        SlowAF = 5     // 0.5x (2.2 addition)
    };

    enum class GravityDirection : int {
        Down = 0,
        Up = 1
    };

    struct HitboxRect {
        double x, y, width, height;

        bool intersects(const HitboxRect& other) const {
            return !(x + width < other.x || other.x + other.width < x ||
                     y + height < other.y || other.y + other.height < y);
        }

        bool containsPoint(double px, double py) const {
            return px >= x && px <= x + width && py >= y && py <= y + height;
        }
    };

    struct SimObjectData {
        int objectID;
        double posX, posY;
        double width, height;
        double rotation;
        bool isHazard;
        bool isSolid;
        bool isPortal;
        bool isPad;
        bool isOrb;
        bool isSlope;
        bool isTrigger;
        int portalType;     // 0=none, 1=cube, 2=ship, 3=ball, 4=ufo, 5=wave, 6=robot, 7=spider, 8=swing
        int speedType;      // -1=none, 0-5 speed settings
        int gravityType;    // -1=none, 0=normal, 1=flipped
        int padType;        // 0=yellow, 1=pink, 2=red, 3=blue
        int orbType;
        double slopeAngle;  // angle of slope surface
        bool isWarpPortal;
        double warpTargetX;

        // Groups and trigger info
        int groupID;
        bool isActive;

        HitboxRect getHitbox() const {
            double hw = width * 0.5;
            double hh = height * 0.5;
            return {posX - hw, posY - hh, width, height};
        }

        HitboxRect getInnerHitbox() const {
            // Slightly smaller hitbox for hazards (matching GD's actual behavior)
            double factor = isHazard ? 0.7 : 1.0;
            double w = width * factor;
            double h = height * factor;
            return {posX - w * 0.5, posY - h * 0.5, w, h};
        }
    };

    struct PlayerState {
        double posX = 0.0;
        double posY = 105.0;  // Default ground level
        double velocityX = 0.0;
        double velocityY = 0.0;
        double rotation = 0.0;
        double rotationSpeed = 0.0;

        GameMode gameMode = GameMode::Cube;
        PlayerSize playerSize = PlayerSize::Normal;
        SpeedSetting speed = SpeedSetting::Normal;
        GravityDirection gravity = GravityDirection::Down;

        bool isOnGround = true;
        bool isDead = false;
        bool isHolding = false;
        bool wasHolding = false;
        bool hasBufferedClick = false;
        bool isDashing = false;

        // Robot-specific
        bool robotJumping = false;
        double robotJumpTime = 0.0;
        static constexpr double ROBOT_MAX_JUMP_TIME = 0.25;

        // Spider-specific
        bool spiderTeleported = false;

        // Swing-specific
        int swingDirection = 1; // 1 = up swing on click, -1 = down swing

        int currentFrame = 0;
        double percentComplete = 0.0;

        HitboxRect getHitbox() const {
            double size = (playerSize == PlayerSize::Mini) ? 15.0 : 30.0;
            // Adjust hitbox by gamemode
            double w = size;
            double h = size;
            switch (gameMode) {
                case GameMode::Ship:
                case GameMode::Swing:
                    w = size * 1.2;
                    h = size * 0.7;
                    break;
                case GameMode::Wave:
                    w = size * 0.6;
                    h = size * 0.6;
                    break;
                default:
                    break;
            }
            return {posX - w * 0.5, posY - h * 0.5, w, h};
        }
    };

    struct FrameData {
        int frame;
        double x;
        double y;
        double rotation;
        bool isPressed;
    };

    // GD 2.2 physics constants
    namespace Physics {
        // Gravity (pixels/tick^2 at 240 TPS effectively)
        constexpr double GRAVITY_NORMAL = -0.958199;
        constexpr double GRAVITY_MINI = -0.80;

        // Max fall speeds
        constexpr double MAX_FALL_SPEED = -15.0;
        constexpr double MAX_FALL_SPEED_MINI = -12.0;

        // Jump velocities per gamemode
        constexpr double CUBE_JUMP_VELOCITY = 11.1806;
        constexpr double CUBE_JUMP_VELOCITY_MINI = 8.8;

        constexpr double BALL_SWITCH_VELOCITY = 9.2;
        constexpr double BALL_SWITCH_VELOCITY_MINI = 7.6;

        constexpr double UFO_CLICK_VELOCITY = 7.8;
        constexpr double UFO_CLICK_VELOCITY_MINI = 6.4;

        constexpr double ROBOT_JUMP_VELOCITY = 11.1806;
        constexpr double ROBOT_JUMP_VELOCITY_MINI = 8.8;
        constexpr double ROBOT_GRAVITY_HOLD = -0.45;

        constexpr double SPIDER_TELEPORT_BOOST = 5.0;

        // Ship gravity
        constexpr double SHIP_GRAVITY_UP = 0.8;
        constexpr double SHIP_GRAVITY_DOWN = -0.6;
        constexpr double SHIP_MAX_UP = 8.0;
        constexpr double SHIP_MAX_DOWN = -7.5;
        constexpr double SHIP_GRAVITY_UP_MINI = 0.65;
        constexpr double SHIP_GRAVITY_DOWN_MINI = -0.5;

        // Wave speed
        constexpr double WAVE_SLOPE_UP = 1.0;     // ratio: for every 1 pixel X, wave goes +1 Y
        constexpr double WAVE_SLOPE_DOWN = -1.0;

        // Swing
        constexpr double SWING_CLICK_VELOCITY = 7.0;
        constexpr double SWING_GRAVITY = -0.7;
        constexpr double SWING_MAX_SPEED = 8.0;

        // Speed multipliers (pixels per tick at 240 TPS, base derived from 60Hz equivalents)
        inline double getSpeedMultiplier(SpeedSetting s) {
            switch (s) {
                case SpeedSetting::SlowAF:  return 5.770 * 0.5 / 4.0;
                case SpeedSetting::Slow:    return 5.770 * 0.7 / 4.0;
                case SpeedSetting::Normal:  return 5.770 / 4.0;
                case SpeedSetting::Fast:    return 5.770 * 1.2 / 4.0;
                case SpeedSetting::Faster:  return 5.770 * 1.5 / 4.0;
                case SpeedSetting::Fastest: return 5.770 * 1.8 / 4.0;
                default: return 5.770 / 4.0;
            }
        }

        // Ground and ceiling Y
        constexpr double GROUND_Y = 105.0;
        constexpr double CEILING_Y = 1395.0;  // Typical level height
        constexpr double GROUND_Y_MINI = 105.0;
    }

    class PhysicsSimulator {
    public:
        PhysicsSimulator();
        ~PhysicsSimulator();

        void setObjects(std::vector<SimObjectData> objects);
        void setLevelLength(double length);
        void setStartGameMode(GameMode mode);
        void setStartSpeed(SpeedSetting speed);
        void setStartGravity(GravityDirection grav);
        void setStartMini(bool mini);
        void setTPS(int tps);

        void reset();
        PlayerState& getState();
        const PlayerState& getState() const;

        // Core simulation step
        void step(bool holding);

        // Check if player reached end
        bool hasReachedEnd() const;

        // Get progress percentage
        double getProgress() const;

        // Get all frame data recorded so far
        const std::vector<FrameData>& getFrameData() const;

    private:
        void applyGameModePhysics(bool holding);
        void applyCubePhysics(bool holding);
        void applyShipPhysics(bool holding);
        void applyBallPhysics(bool holding);
        void applyUFOPhysics(bool holding);
        void applyWavePhysics(bool holding);
        void applyRobotPhysics(bool holding);
        void applySpiderPhysics(bool holding);
        void applySwingPhysics(bool holding);

        void applyGravity();
        void movePlayer();
        void checkCollisions();
        void checkPortals();
        void checkPadsAndOrbs(bool holding);
        void clampPosition();
        void updateRotation();
        void handleSlopePhysics(const SimObjectData& slope);

        double getGravityForCurrentMode() const;
        double getJumpVelocity() const;
        double getGravitySign() const;

        // Spatial hash for efficient collision detection
        void buildSpatialHash();
        std::vector<int> getObjectsNearPlayer() const;

        PlayerState m_state;
        std::vector<SimObjectData> m_objects;
        std::vector<FrameData> m_frameData;
        double m_levelLength = 10000.0;
        int m_tps = 240;
        double m_dt = 1.0 / 240.0;

        // Spatial hash
        static constexpr double CELL_SIZE = 120.0;
        std::unordered_map<int64_t, std::vector<int>> m_spatialHash;

        int64_t hashCell(int cx, int cy) const {
            return (static_cast<int64_t>(cx) << 32) | static_cast<int64_t>(static_cast<uint32_t>(cy));
        }
    };

} // namespace pathfinder