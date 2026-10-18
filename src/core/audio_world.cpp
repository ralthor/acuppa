#include "acuppa/audio_world.hpp"

#include <miniaudio.h>

#include <glm/geometric.hpp>

#include <cmath>
#include <map>
#include <stdexcept>
#include <string>

namespace acuppa {
namespace fs = std::filesystem;
namespace {

bool finite(glm::vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

void require_volume(float volume) {
    if (!std::isfinite(volume) || volume < 0 || volume > 4)
        throw std::invalid_argument("sound volume must be from 0 to 4");
}

void require_pitch(float pitch) {
    if (!std::isfinite(pitch) || pitch < 0.25f || pitch > 4.0f)
        throw std::invalid_argument("sound pitch must be from 0.25 to 4");
}

std::runtime_error audio_error(const std::string& action, ma_result result) {
    return std::runtime_error(action + ": " + ma_result_description(result));
}

}  // namespace

struct AudioWorld::Impl {
    explicit Impl(bool use_output_device) {
        if (use_output_device && ma_engine_init(nullptr, &engine) == MA_SUCCESS) {
            const ma_device* device = ma_engine_get_device(&engine);
            if (device && device->pContext && device->pContext->backend != ma_backend_null)
                return;
            ma_engine_uninit(&engine);
        }
        offline = true;
        ma_engine_config config = ma_engine_config_init();
        config.noDevice = MA_TRUE;
        config.channels = 2;
        config.sampleRate = 48000;
        const ma_result result = ma_engine_init(&config, &engine);
        if (result != MA_SUCCESS) throw audio_error("cannot initialize audio engine", result);
    }

    ~Impl() {
        for (auto& [_, entry] : sounds) ma_sound_uninit(&entry->sound);
        ma_engine_uninit(&engine);
    }

    struct Entry {
        ma_sound sound{};
        SoundState state;
    };

    Entry& require(uint64_t id) {
        const auto found = sounds.find(id);
        if (found == sounds.end()) throw std::invalid_argument("unknown sound ID");
        return *found->second;
    }

    void prune_finished() {
        for (auto it = sounds.begin(); it != sounds.end();) {
            if (!ma_sound_at_end(&it->second->sound)) { ++it; continue; }
            ma_sound_uninit(&it->second->sound);
            it = sounds.erase(it);
        }
    }

    ma_engine engine{};
    bool offline = false;
    uint64_t next_id = 1;
    std::map<uint64_t, std::unique_ptr<Entry>> sounds;
};

AudioWorld::AudioWorld(bool use_output_device)
    : impl_(std::make_unique<Impl>(use_output_device)) {}
AudioWorld::~AudioWorld() = default;

uint64_t AudioWorld::play(const fs::path& file, SoundState settings) {
    require_volume(settings.volume);
    require_pitch(settings.pitch);
    if (!finite(settings.position)) throw std::invalid_argument("sound position must be finite");
    if (!fs::is_regular_file(file) || fs::file_size(file) > 16 * 1024 * 1024)
        throw std::invalid_argument("sound file is missing or exceeds 16 MiB");
    impl_->prune_finished();
    if (impl_->sounds.size() >= 128) throw std::runtime_error("active sound limit reached");
    auto entry = std::make_unique<Impl::Entry>();
    const auto path = file.string();
    const ma_result loaded = ma_sound_init_from_file(&impl_->engine, path.c_str(),
        MA_SOUND_FLAG_STREAM, nullptr, nullptr, &entry->sound);
    if (loaded != MA_SUCCESS) throw audio_error("cannot load sound '" + path + "'", loaded);
    ma_sound_set_spatialization_enabled(&entry->sound, settings.spatial ? MA_TRUE : MA_FALSE);
    ma_sound_set_position(&entry->sound, settings.position.x, settings.position.y,
                          settings.position.z);
    ma_sound_set_volume(&entry->sound, settings.volume);
    ma_sound_set_pitch(&entry->sound, settings.pitch);
    ma_sound_set_looping(&entry->sound, settings.looping ? MA_TRUE : MA_FALSE);
    const ma_result started = ma_sound_start(&entry->sound);
    if (started != MA_SUCCESS) {
        ma_sound_uninit(&entry->sound);
        throw audio_error("cannot start sound '" + path + "'", started);
    }
    entry->state = settings;
    const uint64_t id = impl_->next_id++;
    impl_->sounds.emplace(id, std::move(entry));
    return id;
}

void AudioWorld::stop(uint64_t id) {
    auto& entry = impl_->require(id);
    ma_sound_stop(&entry.sound);
    ma_sound_uninit(&entry.sound);
    impl_->sounds.erase(id);
}

void AudioWorld::set_volume(uint64_t id, float volume) {
    require_volume(volume);
    auto& entry = impl_->require(id);
    ma_sound_set_volume(&entry.sound, volume);
    entry.state.volume = volume;
}

void AudioWorld::set_pitch(uint64_t id, float pitch) {
    require_pitch(pitch);
    auto& entry = impl_->require(id);
    ma_sound_set_pitch(&entry.sound, pitch);
    entry.state.pitch = pitch;
}

void AudioWorld::set_position(uint64_t id, glm::vec3 position) {
    if (!finite(position)) throw std::invalid_argument("sound position must be finite");
    auto& entry = impl_->require(id);
    ma_sound_set_position(&entry.sound, position.x, position.y, position.z);
    entry.state.position = position;
}

void AudioWorld::set_listener(glm::vec3 position, glm::vec3 forward, glm::vec3 up) {
    const float forward_length = glm::length(forward);
    const float up_length = glm::length(up);
    const float cross_length = glm::length(glm::cross(forward, up));
    if (!finite(position) || !finite(forward) || !finite(up) ||
        !std::isfinite(forward_length) || !std::isfinite(up_length) ||
        !std::isfinite(cross_length) || forward_length < 0.001f ||
        up_length < 0.001f || cross_length < 0.001f)
        throw std::invalid_argument("listener needs finite position and independent directions");
    forward = glm::normalize(forward);
    up = glm::normalize(up);
    ma_engine_listener_set_position(&impl_->engine, 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(&impl_->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&impl_->engine, 0, up.x, up.y, up.z);
}

std::optional<SoundState> AudioWorld::state(uint64_t id) const {
    const auto found = impl_->sounds.find(id);
    if (found == impl_->sounds.end()) return std::nullopt;
    SoundState result = found->second->state;
    result.playing = ma_sound_is_playing(&found->second->sound) &&
                     !ma_sound_at_end(&found->second->sound);
    return result;
}

bool AudioWorld::output_available() const { return !impl_->offline; }

std::vector<float> AudioWorld::render_offline(uint64_t frames) {
    if (!impl_->offline) throw std::logic_error("offline audio render requires no device");
    if (frames > 480000) throw std::invalid_argument("offline audio render exceeds 10 seconds");
    std::vector<float> output(size_t(frames) * 2);
    const ma_result result = ma_engine_read_pcm_frames(&impl_->engine, output.data(), frames,
                                                       nullptr);
    if (result != MA_SUCCESS) throw audio_error("cannot render audio", result);
    return output;
}

}  // namespace acuppa
