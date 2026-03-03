#include "PathfinderEngine.hpp"
#include <algorithm>
#include <chrono>
#include <numeric>

namespace pathfinder {

    PathfinderEngine::PathfinderEngine()
        : m_rng(std::chrono::steady_clock::now().time_since_epoch().count()) {
    }

    PathfinderEngine::~PathfinderEngine() {
        cancel();
    }

    void PathfinderEngine::setConfig(const PathfinderConfig& config) {
        m_config = config;
    }

    void PathfinderEngine::setLevelData(std::vector<SimObjectData> objects, double levelLength,
                                         GameMode startMode, SpeedSetting startSpeed,
                                         GravityDirection startGravity, bool startMini) {
        m_objects = std::move(objects);
        m_levelLength = levelLength;
        m_startMode = startMode;
        m_startSpeed = startSpeed;
        m_startGravity = startGravity;
        m_startMini = startMini;
    }

    void PathfinderEngine::start() {
        if (m_status == PathfinderStatus::Running) return;

        cancel(); // Ensure previous thread is done

        m_cancelFlag = false;
        m_status = PathfinderStatus::Running;
        m_progress = 0.0;

        m_thread = std::make_unique<std::thread>(&PathfinderEngine::runPathfinder, this);
        m_thread->detach();
    }

    void PathfinderEngine::cancel() {
        m_cancelFlag = true;
        if (m_thread && m_thread->joinable()) {
            m_thread->join();
        }
        m_thread.reset();
        m_status = PathfinderStatus::Cancelled;
    }

    PathfinderStatus PathfinderEngine::getStatus() const {
        return m_status.load();
    }

    double PathfinderEngine::getProgress() const {
        return m_progress.load();
    }

    std::string PathfinderEngine::getStatusText() const {
        switch (m_status.load()) {
            case PathfinderStatus::Idle:      return "Ready";
            case PathfinderStatus::Running:   return "Analyzing...";
            case PathfinderStatus::Success:   return "Path Found!";
            case PathfinderStatus::Failed:    return "No Path Found";
            case PathfinderStatus::Cancelled: return "Cancelled";
            default: return "Unknown";
        }
    }

    bool PathfinderEngine::hasResult() const {
        std::lock_guard<std::mutex> lock(m_resultMutex);
        return !m_result.empty();
    }

    std::vector<FrameData> PathfinderEngine::getResult() const {
        std::lock_guard<std::mutex> lock(m_resultMutex);
        return m_result;
    }

    void PathfinderEngine::setProgressCallback(std::function<void(double)> callback) {
        m_progressCallback = std::move(callback);
    }

    void PathfinderEngine::setCompletionCallback(std::function<void(bool)> callback) {
        m_completionCallback = std::move(callback);
    }

    void PathfinderEngine::runPathfinder() {
        try {
            // Primary: Beam search with look-ahead
            auto result = runBeamSearch();

            if (m_cancelFlag) {
                m_status = PathfinderStatus::Cancelled;
                return;
            }

            if (!result.empty()) {
                std::lock_guard<std::mutex> lock(m_resultMutex);
                m_result = std::move(result);
                m_status = PathfinderStatus::Success;
                m_progress = 100.0;

                if (m_completionCallback) {
                    m_completionCallback(true);
                }
            } else {
                m_status = PathfinderStatus::Failed;
                if (m_completionCallback) {
                    m_completionCallback(false);
                }
            }
        } catch (...) {
            m_status = PathfinderStatus::Failed;
            if (m_completionCallback) {
                m_completionCallback(false);
            }
        }
    }

    double PathfinderEngine::evaluateState(const PlayerState& state, int frame, bool justPressed) const {
        if (state.isDead) return -1000000.0;

        double score = state.posX * 10.0; // Primary: maximize progress

        // Prefer being near the center vertically (avoid extremes)
        double centerY = (Physics::GROUND_Y + Physics::CEILING_Y) * 0.5;
        double yDist = std::abs(state.posY - centerY);
        score -= yDist * 0.01;

        // Penalty for high velocity (unstable states)
        score -= std::abs(state.velocityY) * 0.1;

        // Bonus for being on ground in ground-based modes
        if (state.isOnGround &&
            (state.gameMode == GameMode::Cube || state.gameMode == GameMode::Ball ||
             state.gameMode == GameMode::Robot || state.gameMode == GameMode::Spider)) {
            score += 5.0;
        }

        // Completion bonus
        if (state.percentComplete >= 100.0) {
            score += 1000000.0;
        }

        return score;
    }

    std::vector<FrameData> PathfinderEngine::runBeamSearch() {
        constexpr int BEAM_WIDTH = 32;
        constexpr int DECISION_INTERVAL = 4; // Make decisions every N frames

        // Estimate total frames
        double xSpeedPerFrame = Physics::getSpeedMultiplier(m_startSpeed);
        int estimatedFrames = static_cast<int>(m_levelLength / xSpeedPerFrame) + 100;
        estimatedFrames = std::min(estimatedFrames, 1000000); // Safety cap

        // Initialize beam with starting state
        std::vector<BeamState> beam;
        {
            BeamState initial;
            PhysicsSimulator sim;
            sim.setObjects(m_objects);
            sim.setLevelLength(m_levelLength);
            sim.setStartGameMode(m_startMode);
            sim.setStartSpeed(m_startSpeed);
            sim.setStartGravity(m_startGravity);
            sim.setStartMini(m_startMini);
            sim.setTPS(m_config.tps);
            sim.reset();

            initial.playerState = sim.getState();
            initial.score = 0.0;
            beam.push_back(std::move(initial));
        }

        int frame = 0;
        double bestProgressSoFar = 0.0;
        int staleFrames = 0;

        while (frame < estimatedFrames && !m_cancelFlag) {
            std::vector<BeamState> candidates;
            candidates.reserve(beam.size() * 2);

            for (auto& state : beam) {
                if (state.playerState.isDead) continue;
                if (state.playerState.posX >= m_levelLength) {
                    // Found a solution
                    PhysicsSimulator sim;
                    sim.setObjects(m_objects);
                    sim.setLevelLength(m_levelLength);
                    sim.setStartGameMode(m_startMode);
                    sim.setStartSpeed(m_startSpeed);
                    sim.setStartGravity(m_startGravity);
                    sim.setStartMini(m_startMini);
                    sim.setTPS(m_config.tps);
                    sim.reset();

                    // Replay the inputs to get frame data
                    for (size_t i = 0; i < state.inputHistory.size(); i++) {
                        sim.step(state.inputHistory[i]);
                        if (sim.getState().isDead) break;
                    }

                    return sim.getFrameData();
                }

                // Try both: pressing and not pressing
                for (int action = 0; action < 2; action++) {
                    bool press = (action == 1);

                    BeamState candidate;
                    candidate.inputHistory = state.inputHistory;

                    // Simulate DECISION_INTERVAL frames with this action
                    PhysicsSimulator sim;
                    sim.setObjects(m_objects);
                    sim.setLevelLength(m_levelLength);
                    sim.setStartGameMode(m_startMode);
                    sim.setStartSpeed(m_startSpeed);
                    sim.setStartGravity(m_startGravity);
                    sim.setStartMini(m_startMini);
                    sim.setTPS(m_config.tps);
                    sim.reset();

                    // Fast-forward to current state by replaying history
                    auto& simState = sim.getState();
                    simState = state.playerState;
                    simState.currentFrame = static_cast<int>(state.inputHistory.size());

                    bool died = false;
                    for (int step = 0; step < DECISION_INTERVAL && !died; step++) {
                        sim.step(press);
                        candidate.inputHistory.push_back(press);
                        if (sim.getState().isDead) {
                            died = true;
                        }
                    }

                    if (died) {
                        candidate.score = -1000000.0;
                    } else {
                        // Look ahead to evaluate safety
                        PhysicsSimulator lookAheadSim;
                        lookAheadSim.setObjects(m_objects);
                        lookAheadSim.setLevelLength(m_levelLength);
                        lookAheadSim.setStartGameMode(m_startMode);
                        lookAheadSim.setStartSpeed(m_startSpeed);
                        lookAheadSim.setStartGravity(m_startGravity);
                        lookAheadSim.setStartMini(m_startMini);
                        lookAheadSim.setTPS(m_config.tps);
                        lookAheadSim.reset();

                        auto& laState = lookAheadSim.getState();
                        laState = sim.getState();

                        bool safeAhead = true;
                        int laFrames = std::min(m_config.lookAheadFrames, 60);
                        for (int la = 0; la < laFrames; la++) {
                            lookAheadSim.step(false); // Coast without input
                            if (lookAheadSim.getState().isDead) {
                                safeAhead = false;
                                break;
                            }
                        }

                        candidate.score = evaluateState(sim.getState(),
                            static_cast<int>(candidate.inputHistory.size()), press);
                        if (!safeAhead) {
                            candidate.score -= 500.0; // Penalty for unsafe future
                        }
                    }

                    candidate.playerState = sim.getState();
                    candidates.push_back(std::move(candidate));
                }

                // Also try: press then release pattern, and release then press
                if (DECISION_INTERVAL >= 2) {
                    for (int pattern = 0; pattern < 2; pattern++) {
                        BeamState candidate;
                        candidate.inputHistory = state.inputHistory;

                        PhysicsSimulator sim;
                        sim.setObjects(m_objects);
                        sim.setLevelLength(m_levelLength);
                        sim.setStartGameMode(m_startMode);
                        sim.setStartSpeed(m_startSpeed);
                        sim.setStartGravity(m_startGravity);
                        sim.setStartMini(m_startMini);
                        sim.setTPS(m_config.tps);
                        sim.reset();

                        auto& simState = sim.getState();
                        simState = state.playerState;
                        simState.currentFrame = static_cast<int>(state.inputHistory.size());

                        bool died = false;
                        for (int step = 0; step < DECISION_INTERVAL && !died; step++) {
                            bool press;
                            if (pattern == 0) {
                                press = (step < DECISION_INTERVAL / 2);
                            } else {
                                press = (step >= DECISION_INTERVAL / 2);
                            }
                            sim.step(press);
                            candidate.inputHistory.push_back(press);
                            if (sim.getState().isDead) died = true;
                        }

                        candidate.playerState = sim.getState();
                        candidate.score = died ? -1000000.0 :
                            evaluateState(sim.getState(),
                                static_cast<int>(candidate.inputHistory.size()), false);

                        candidates.push_back(std::move(candidate));
                    }
                }
            }

            // Sort by score descending and keep top BEAM_WIDTH
            std::sort(candidates.begin(), candidates.end(),
                [](const BeamState& a, const BeamState& b) {
                    return a.score > b.score;
                });

            beam.clear();
            int kept = 0;
            for (auto& c : candidates) {
                if (c.playerState.isDead && kept > 0) continue; // Skip dead unless beam is empty
                beam.push_back(std::move(c));
                kept++;
                if (kept >= BEAM_WIDTH) break;
            }

            // If beam is empty, all paths died
            if (beam.empty()) {
                return {};
            }

            frame += DECISION_INTERVAL;

            // Update progress
            double bestProgress = 0.0;
            for (const auto& s : beam) {
                double prog = s.playerState.posX / m_levelLength * 100.0;
                bestProgress = std::max(bestProgress, prog);
            }

            m_progress = bestProgress;
            if (m_progressCallback) {
                m_progressCallback(bestProgress);
            }

            // Stale detection
            if (bestProgress <= bestProgressSoFar + 0.01) {
                staleFrames += DECISION_INTERVAL;
                if (staleFrames > m_config.tps * 5) { // 5 seconds of no progress
                    // Try random exploration
                    for (auto& s : beam) {
                        if (m_rng() % 3 == 0) {
                            int randFrames = m_rng() % 20 + 5;
                            PhysicsSimulator sim;
                            sim.setObjects(m_objects);
                            sim.setLevelLength(m_levelLength);
                            sim.setStartGameMode(m_startMode);
                            sim.setStartSpeed(m_startSpeed);
                            sim.setStartGravity(m_startGravity);
                            sim.setStartMini(m_startMini);
                            sim.setTPS(m_config.tps);
                            sim.reset();

                            auto& simState = sim.getState();
                            simState = s.playerState;

                            for (int r = 0; r < randFrames; r++) {
                                bool randomPress = (m_rng() % 2 == 0);
                                sim.step(randomPress);
                                s.inputHistory.push_back(randomPress);
                                if (sim.getState().isDead) break;
                            }
                            s.playerState = sim.getState();
                            s.score = evaluateState(s.playerState,
                                static_cast<int>(s.inputHistory.size()), false);
                        }
                    }
                }
                if (staleFrames > m_config.tps * 30) { // 30 seconds
                    return {}; // Give up
                }
            } else {
                bestProgressSoFar = bestProgress;
                staleFrames = 0;
            }
        }

        // If we got here, check if best beam state reached end
        for (auto& s : beam) {
            if (s.playerState.posX >= m_levelLength) {
                PhysicsSimulator sim;
                sim.setObjects(m_objects);
                sim.setLevelLength(m_levelLength);
                sim.setStartGameMode(m_startMode);
                sim.setStartSpeed(m_startSpeed);
                sim.setStartGravity(m_startGravity);
                sim.setStartMini(m_startMini);
                sim.setTPS(m_config.tps);
                sim.reset();

                for (size_t i = 0; i < s.inputHistory.size(); i++) {
                    sim.step(s.inputHistory[i]);
                    if (sim.getState().isDead) break;
                }
                return sim.getFrameData();
            }
        }

        return {};
    }

    // Genetic algorithm methods (used as fallback or alternative)
    double PathfinderEngine::evaluateGenome(const Genome& genome) {
        PhysicsSimulator sim;
        sim.setObjects(m_objects);
        sim.setLevelLength(m_levelLength);
        sim.setStartGameMode(m_startMode);
        sim.setStartSpeed(m_startSpeed);
        sim.setStartGravity(m_startGravity);
        sim.setStartMini(m_startMini);
        sim.setTPS(m_config.tps);
        sim.reset();

        for (size_t i = 0; i < genome.inputs.size() && !sim.getState().isDead; i++) {
            sim.step(genome.inputs[i]);
        }

        const auto& state = sim.getState();
        double fitness = state.posX; // Base: distance traveled

        if (state.isDead) {
            fitness *= 0.5; // Penalty for dying
        }

        if (sim.hasReachedEnd()) {
            fitness += m_levelLength * 10.0; // Huge bonus for completing
        }

        return fitness;
    }

    PathfinderEngine::Genome PathfinderEngine::crossover(const Genome& a, const Genome& b) {
        Genome child;
        size_t len = std::max(a.inputs.size(), b.inputs.size());
        child.inputs.resize(len);

        // Two-point crossover
        std::uniform_int_distribution<size_t> dist(0, len > 0 ? len - 1 : 0);
        size_t p1 = dist(m_rng);
        size_t p2 = dist(m_rng);
        if (p1 > p2) std::swap(p1, p2);

        for (size_t i = 0; i < len; i++) {
            bool fromA = (i < p1 || i > p2);
            if (fromA) {
                child.inputs[i] = (i < a.inputs.size()) ? a.inputs[i] : false;
            } else {
                child.inputs[i] = (i < b.inputs.size()) ? b.inputs[i] : false;
            }
        }

        return child;
    }

    void PathfinderEngine::mutate(Genome& genome) {
        std::uniform_real_distribution<double> prob(0.0, 1.0);
        for (auto& input : genome.inputs) {
            if (prob(m_rng) < m_config.mutationRate) {
                input = !input;
            }
        }

        // Occasionally insert or remove a sequence of presses
        if (prob(m_rng) < 0.1 && genome.inputs.size() > 10) {
            std::uniform_int_distribution<size_t> posDist(0, genome.inputs.size() - 1);
            size_t start = posDist(m_rng);
            size_t length = std::min(static_cast<size_t>(m_rng() % 20 + 1),
                                     genome.inputs.size() - start);
            bool val = (m_rng() % 2 == 0);
            for (size_t i = start; i < start + length; i++) {
                genome.inputs[i] = val;
            }
        }
    }

    std::vector<bool> PathfinderEngine::generateGreedyInputs() {
        PhysicsSimulator sim;
        sim.setObjects(m_objects);
        sim.setLevelLength(m_levelLength);
        sim.setStartGameMode(m_startMode);
        sim.setStartSpeed(m_startSpeed);
        sim.setStartGravity(m_startGravity);
        sim.setStartMini(m_startMini);
        sim.setTPS(m_config.tps);
        sim.reset();

        std::vector<bool> inputs;

        double xSpeedPerFrame = Physics::getSpeedMultiplier(m_startSpeed);
        int maxFrames = static_cast<int>(m_levelLength / xSpeedPerFrame) + 1000;

        for (int i = 0; i < maxFrames && !sim.getState().isDead && !sim.hasReachedEnd(); i++) {
            // Simple greedy: try pressing, if it leads to death within lookAhead, don't press
            PhysicsSimulator pressSim;
            pressSim.setObjects(m_objects);
            pressSim.setLevelLength(m_levelLength);
            pressSim.setStartGameMode(m_startMode);
            pressSim.setStartSpeed(m_startSpeed);
            pressSim.setStartGravity(m_startGravity);
            pressSim.setStartMini(m_startMini);
            pressSim.setTPS(m_config.tps);
            pressSim.reset();
            pressSim.getState() = sim.getState();
            pressSim.step(true);

            PhysicsSimulator noPressSim;
            noPressSim.setObjects(m_objects);
            noPressSim.setLevelLength(m_levelLength);
            noPressSim.setStartGameMode(m_startMode);
            noPressSim.setStartSpeed(m_startSpeed);
            noPressSim.setStartGravity(m_startGravity);
            noPressSim.setStartMini(m_startMini);
            noPressSim.setTPS(m_config.tps);
            noPressSim.reset();
            noPressSim.getState() = sim.getState();
            noPressSim.step(false);

            double pressScore = evaluateState(pressSim.getState(), i, true);
            double noPressScore = evaluateState(noPressSim.getState(), i, false);

            bool shouldPress = (pressScore > noPressScore);
            inputs.push_back(shouldPress);
            sim.step(shouldPress);
        }

        return inputs;
    }

} // namespace pathfinder