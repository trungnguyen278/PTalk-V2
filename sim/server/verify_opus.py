# File:    verify_opus.py
# Author:  Trung Nguyen
# GitHub:  https://github.com/trungnguyen278/PTalk-V2
# Date:    30 Jun 2026
#
# Description:
#  - Part of the PTalk-V2 project
#  - Written and maintained by Trung Nguyen

"""Verify Opus encode/decode quality on PC.
Encodes a WAV file with the same settings as the server,
decodes it, and saves the result for listening comparison.
"""
import wave, struct, sys, os
import numpy as np

if os.name == 'nt':
    current_dir = os.path.dirname(os.path.realpath(__file__))
    os.environ['PATH'] = current_dir + os.pathsep + os.environ.get('PATH', '')
    if hasattr(os, 'add_dll_directory'):
        os.add_dll_directory(current_dir)

import opuslib

SAMPLE_RATE = 16000
FRAME_SIZE = 320  # 20ms

def test_opus(wav_path, bitrate=64000):
    # Load WAV
    with wave.open(wav_path, 'r') as wf:
        sr = wf.getframerate()
        nch = wf.getnchannels()
        sw = wf.getsampwidth()
        raw = wf.readframes(wf.getnframes())

    data = np.frombuffer(raw, dtype=np.int16)
    if nch > 1:
        data = data.reshape(-1, nch)[:, 0]

    # Resample if needed
    if sr != SAMPLE_RATE:
        ratio = SAMPLE_RATE / sr
        indices = np.arange(0, len(data), 1 / ratio).astype(int)
        indices = indices[indices < len(data)]
        data = data[indices]

    data = data.astype(np.int16)
    print(f"Input: {len(data)} samples, {len(data)/SAMPLE_RATE:.1f}s")

    # Encode
    encoder = opuslib.Encoder(SAMPLE_RATE, 1, opuslib.APPLICATION_AUDIO)
    encoder.bitrate = bitrate
    encoder.complexity = 10

    encoded_frames = []
    for i in range(0, len(data) - FRAME_SIZE + 1, FRAME_SIZE):
        chunk = data[i:i + FRAME_SIZE]
        encoded = encoder.encode(chunk.tobytes(), FRAME_SIZE)
        encoded_frames.append(encoded)

    total_encoded_bytes = sum(len(f) for f in encoded_frames)
    print(f"Encoded: {len(encoded_frames)} frames, {total_encoded_bytes} bytes, "
          f"avg {total_encoded_bytes/len(encoded_frames):.0f} bytes/frame")

    # Decode
    decoder = opuslib.Decoder(SAMPLE_RATE, 1)
    decoded_frames = []
    frame_disc_count = 0
    prev_last = 0

    for idx, frame in enumerate(encoded_frames):
        pcm_bytes = decoder.decode(frame, FRAME_SIZE)
        samples = np.frombuffer(pcm_bytes, dtype=np.int16)
        decoded_frames.append(samples)

        # Check frame boundary discontinuity
        if idx > 0:
            diff = int(samples[0]) - int(prev_last)
            if abs(diff) > 500:
                frame_disc_count += 1
                if frame_disc_count <= 10:
                    print(f"  FRAME_DISC[{idx}]: {prev_last} -> {samples[0]} (diff={diff})")
        prev_last = int(samples[-1])

    print(f"Frame boundary discontinuities (>500): {frame_disc_count}/{len(encoded_frames)}")

    # Save decoded output
    decoded_all = np.concatenate(decoded_frames)
    out_path = wav_path.replace('.wav', f'_opus{bitrate//1000}k.wav')
    with wave.open(out_path, 'w') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(decoded_all.tobytes())

    print(f"Saved: {out_path}")
    print(f"Play both files and compare quality!")

if __name__ == "__main__":
    wav_file = sys.argv[1] if len(sys.argv) > 1 else "test_sine_440hz.wav"
    bitrate = int(sys.argv[2]) if len(sys.argv) > 2 else 64000
    test_opus(wav_file, bitrate)
