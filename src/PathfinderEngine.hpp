#pragma once

#include "PhysicsSimulator.hpp"
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <vector>
#include <random>
#include <memory>

namespace pathfinder {

    enum class PathfinderStatus {
        Idle,
        Running,
        Success,
        Failed,
        Cancelled
    };

    struct PathfinderConfig {
        int tps = 240;
        int maxAttempts = 50000;
        int lookAheadFrames = 120;
        double mutationRate = 0.05;
        int populationSize = 64;
        int eliteCount = 8;
    };

    class PathfinderEngine {
    public:
        PathfinderEngine();
        ~PathfinderEngine();

        // Configure
        void setConfig(const PathfinderConfig& config);

        // Set level data
        void setLevelData(std::vector<SimObjectData> objects, double levelLength,
                          GameMode startMode, SpeedSetting startSpeed,
                          GravityDirection startGravity, bool startMini);

        // Start/stop
        void start();
        void cancel();

        // Status
        PathfinderStatus getStatus() const;
        double getProgress() const;
        std::string getStatusText() const;

        // Results
        bool hasResult() const;
        std::vector<FrameData> getResult() const;

        // Callbacks
        void setProgressCallback(std::function<void(double)> callback);
        void setCompletionCallback(std::function<void(bool)> callback);

    private:
        void runPathfinder();

        // Genetic algorithm approach
        struct Genome {
            std::vector<bool> inputs; // One bool per frame: press or not
            double fitness = 0.0;
            double progress = 0.0;
            bool completed = false;
        };

        double evaluateGenome(const Genome& genome);
        Genome crossover(const Genome& a, const Genome& b);
        void mutate(Genome& genome);
        std::vector<bool> generateGreedyInputs();

        // Beam search approach (primary)
        struct BeamState {
            PlayerState playerState;
            std::vector<bool> inputHistory;
            double score = 0.0;
        };

        std::vector<FrameData> runBeamSearch();
        double evaluateState(const PlayerState& state, int frame, bool justPressed) const;

        PathfinderConfig m_config;
        std::vector<SimObjectData> m_objects;
        double m_levelLength = 0.0;
        GameMode m_startMode = GameMode::Cube;
        SpeedSetting m_startSpeed = SpeedSetting::Normal;
        GravityDirection m_startGravity = GravityDirection::Down;
        bool m_startMini = false;

        std::unique_ptr<std::thread> m_thread;
        std::atomic<PathfinderStatus> m_status{PathfinderStatus::Idle};
        std::atomic<double> m_progress{0.0};
        std::atomic<bool> m_cancelFlag{false};

        mutable std::mutex m_resultMutex;
        std::vector<FrameData> m_result;

        std::function<void(double)> m_progressCallback;
        std::function<void(bool)> m_completionCallback;

        std::mt19937 m_rng;
    };

} // namespace pathfinder