"""Generate the checked-in 3D feature-scene contact cue from code."""

from pathlib import Path
import math
import struct
import wave


output = Path(__file__).resolve().parent.parent / "tests/feature_scene/assets/contact.wav"
sample_rate = 24000
frames = bytearray()
for index in range(sample_rate // 3):
    time = index / sample_rate
    envelope = math.exp(-12 * time)
    tone = math.sin(2 * math.pi * (740 - 310 * time) * time)
    frames += struct.pack("<h", int(18000 * envelope * tone))
with wave.open(str(output), "wb") as sound:
    sound.setnchannels(1)
    sound.setsampwidth(2)
    sound.setframerate(sample_rate)
    sound.writeframes(frames)
