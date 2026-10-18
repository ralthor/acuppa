#include "threepoint/audio_world.hpp"

#include <cmath>
#include <filesystem>
#include <stdexcept>

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

double energy(const std::vector<float>& samples) {
    double total = 0;
    for (float sample : samples) total += double(sample) * sample;
    return total;
}

int main(int argc, char* argv[]) {
    if (argc != 2) throw std::runtime_error("audio fixture path required");
    threepoint::AudioWorld audio(false);
    expect(!audio.output_available(), "offline audio test opened a device");
    audio.set_listener({0, 0, 0}, {0, 0, -1}, {0, 1, 0});
    threepoint::SoundState settings;
    settings.spatial = true;
    settings.looping = true;
    settings.position = {-2, 0, -3};
    const uint64_t id = audio.play(argv[1], settings);
    const auto rendered = audio.render_offline(2400);
    expect(rendered.size() == 4800 && energy(rendered) > 0.01,
           "miniaudio did not decode and mix the WAV fixture");
    double left = 0, right = 0;
    for (size_t i = 0; i < rendered.size(); i += 2) {
        left += std::abs(rendered[i]);
        right += std::abs(rendered[i + 1]);
    }
    expect(left > right * 1.2, "3D sound did not pan toward the left listener channel");
    expect(audio.state(id) && audio.state(id)->playing && audio.state(id)->spatial,
           "playing sound state is unavailable");
    audio.set_pitch(id, 1.5f);
    expect(audio.state(id)->pitch == 1.5f, "sound pitch did not update");
    threepoint::AudioWorld normal_pitch(false);
    normal_pitch.set_listener({0, 0, 0}, {0, 0, -1}, {0, 1, 0});
    settings.pitch = 1.0f;
    normal_pitch.play(argv[1], settings);
    const auto unpitched = normal_pitch.render_offline(2400);
    const auto pitched = audio.render_offline(2400);
    double pitch_difference = 0;
    for (size_t i = 0; i < pitched.size(); ++i)
        pitch_difference += std::abs(pitched[i] - unpitched[i]);
    expect(pitch_difference > 1.0, "sound pitch did not alter decoded PCM output");
    audio.set_position(id, {2, 0, -3});
    audio.set_volume(id, 0);
    expect(audio.state(id)->position.x == 2 && audio.state(id)->volume == 0,
           "sound position or volume did not update");
    expect(energy(audio.render_offline(2400)) < 1e-8, "muted sound still produced audio");
    audio.stop(id);
    expect(!audio.state(id), "stopped sound still exists");
    settings.looping = false;
    const uint64_t finished = audio.play(argv[1], settings);
    audio.render_offline(24000);
    const uint64_t following = audio.play(argv[1], settings);
    expect(!audio.state(finished) && audio.state(following),
           "finished one-shot sound was not reclaimed before the next play");
    audio.stop(following);
    bool rejected = false;
    try { audio.set_volume(id, 1); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "unknown sound ID accepted");
    rejected = false;
    try { audio.set_pitch(id, 2); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "unknown sound pitch target accepted");
    rejected = false;
    try { audio.play(argv[1], threepoint::SoundState{.pitch = 0}); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "invalid initial sound pitch accepted");
    rejected = false;
    try { audio.set_pitch(following, 4.1f); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "out-of-range sound pitch accepted");
    rejected = false;
    try { audio.play("missing.wav", settings); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "missing sound file accepted");
    rejected = false;
    try { audio.play(std::filesystem::path(argv[1]).parent_path() / "test_gradient.hdr", settings); }
    catch (const std::runtime_error&) { rejected = true; }
    expect(rejected, "non-audio asset was decoded as a sound");
    rejected = false;
    try { audio.set_listener({0, 0, 0}, {0, 0, -1}, {0, 0, -1}); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "parallel listener directions accepted");
}
