#include "GDR2Exporter.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>

namespace pathfinder {

    std::string GDR2Exporter::serialize(const GDR2Replay& replay) {
        std::ostringstream json;
        json << std::fixed << std::setprecision(6);

        json << "{\n";
        json << "  \"format\": \"gdr2\",\n";
        json << "  \"version\": 2,\n";
        json << "  \"author\": \"" << replay.author << "\",\n";
        json << "  \"description\": \"" << replay.description << "\",\n";
        json << "  \"duration\": " << replay.duration << ",\n";
        json << "  \"fps\": " << replay.fps << ",\n";
        json << "  \"levelID\": " << replay.levelID << ",\n";
        json << "  \"levelName\": \"" << replay.levelName << "\",\n";

        // Timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        json << "  \"timestamp\": " << time_t_now << ",\n";

        // Bot info
        json << "  \"bot\": {\n";
        json << "    \"name\": \"AI Pathfinder\",\n";
        json << "    \"version\": \"1.0.0\"\n";
        json << "  },\n";

        // Inputs (compressed: only state changes)
        json << "  \"inputs\": [\n";

        bool lastPressed = false;
        bool firstEntry = true;

        for (size_t i = 0; i < replay.frames.size(); i++) {
            const auto& frame = replay.frames[i];

            // Only record state changes (press/release events)
            if (i == 0 || frame.isPressed != lastPressed) {
                if (!firstEntry) {
                    json << ",\n";
                }
                json << "    {\n";
                json << "      \"frame\": " << frame.frame << ",\n";
                json << "      \"x\": " << frame.x << ",\n";
                json << "      \"y\": " << frame.y << ",\n";
                json << "      \"rotation\": " << frame.rotation << ",\n";
                json << "      \"isPressed\": " << (frame.isPressed ? "true" : "false") << ",\n";
                json << "      \"player\": 1,\n";
                json << "      \"button\": 1\n";
                json << "    }";
                firstEntry = false;
            }

            lastPressed = frame.isPressed;
        }

        json << "\n  ],\n";

        // Full frame data for replay visualization
        json << "  \"frames\": [\n";
        firstEntry = true;

        // Write every Nth frame to keep file size manageable
        int stride = std::max(1, replay.fps / 60); // Downsample to ~60fps for positions

        for (size_t i = 0; i < replay.frames.size(); i += stride) {
            const auto& frame = replay.frames[i];
            if (!firstEntry) {
                json << ",\n";
            }
            json << "    {"
                 << "\"frame\":" << frame.frame
                 << ",\"x\":" << frame.x
                 << ",\"y\":" << frame.y
                 << ",\"rotation\":" << frame.rotation
                 << ",\"isPressed\":" << (frame.isPressed ? "true" : "false")
                 << "}";
            firstEntry = false;
        }

        // Always include last frame
        if (!replay.frames.empty()) {
            const auto& lastFrame = replay.frames.back();
            json << ",\n    {"
                 << "\"frame\":" << lastFrame.frame
                 << ",\"x\":" << lastFrame.x
                 << ",\"y\":" << lastFrame.y
                 << ",\"rotation\":" << lastFrame.rotation
                 << ",\"isPressed\":" << (lastFrame.isPressed ? "true" : "false")
                 << "}";
        }

        json << "\n  ]\n";
        json << "}\n";

        return json.str();
    }

    bool GDR2Exporter::saveToFile(const GDR2Replay& replay, const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        file << serialize(replay);
        file.close();

        return file.good();
    }

    std::string GDR2Exporter::getDefaultFilename(const std::string& levelName, int levelID) {
        // Sanitize level name
        std::string safeName;
        for (char c : levelName) {
            if (std::isalnum(c) || c == '_' || c == '-' || c == ' ') {
                safeName += (c == ' ') ? '_' : c;
            }
        }
        if (safeName.empty()) safeName = "level";

        std::ostringstream filename;
        filename << "AI_" << safeName << "_" << levelID << ".gdr2";
        return filename.str();
    }

} // namespace pathfinder