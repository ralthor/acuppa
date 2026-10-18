#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace acuppa {

struct SoundState {
    bool playing = false;
    bool spatial = false;
    bool looping = false;
    float volume = 1;
    float pitch = 1;
    glm::vec3 position{0};
};

// Owns every sound in one scene. Device-free mode renders PCM for deterministic tests.
class AudioWorld {
public:
    explicit AudioWorld(bool use_output_device = true);
    ~AudioWorld();
    AudioWorld(const AudioWorld&) = delete;
    AudioWorld& operator=(const AudioWorld&) = delete;

    uint64_t play(const std::filesystem::path& file, SoundState settings);
    void stop(uint64_t id);
    void set_volume(uint64_t id, float volume);
    void set_pitch(uint64_t id, float pitch);
    void set_position(uint64_t id, glm::vec3 position);
    void set_listener(glm::vec3 position, glm::vec3 forward, glm::vec3 up);
    std::optional<SoundState> state(uint64_t id) const;
    bool output_available() const;
    // Advances device-free playback when the game has no speaker device.
    std::vector<float> render_offline(uint64_t frames);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace acuppa
