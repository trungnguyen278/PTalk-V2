# File:    server.py
# Author:  Trung Nguyen
# GitHub:  https://github.com/trungnguyen278/PTalk-V2
# Date:    30 Jun 2026
#
# Description:
#  - Part of the PTalk-V2 project
#  - Written and maintained by Trung Nguyen

"""
PTalk V2 - Test WebSocket + MQTT Server
========================================
Simulates the PTalk cloud server for local audio testing.

Features:
- WebSocket server on port 8000 at /ws
- Receives encoded audio (Opus or ADPCM) from device
- Echo mode: sends recorded audio back to device
- WAV file mode: sends a WAV file as TTS response
- Saves all recordings as WAV files
- MQTT broker integration (optional)

Usage:
    python server.py [--port 8000] [--codec opus|adpcm] [--echo] [--wav-file path.wav]
"""

import asyncio
import argparse
import json
import struct
import time
import os
import wave
import sys
from pathlib import Path

import websockets

# Try to import opus codec
try:
    if os.name == 'nt':
        # On Windows, python 3.8+ needs add_dll_directory to find DLLs in the current folder.
        # opuslib also relies on PATH being set.
        # The script may be run from another directory (e.g., sim/venv), so we need the ABSOLUTE path of where server.py actually is.
        current_dir = os.path.dirname(os.path.realpath(__file__))
        os.environ['PATH'] = current_dir + os.pathsep + os.environ.get('PATH', '')
        if hasattr(os, 'add_dll_directory'):
            os.add_dll_directory(current_dir)
            
    import opuslib
    HAS_OPUS = True
except (ImportError, Exception):
    HAS_OPUS = False
    print("[WARN] opuslib not available (missing opus DLL or not installed).")
    print("       On Windows: pip install opuslib, then place opus.dll in PATH")
    print("       ADPCM codec will be used instead.")

import numpy as np

SAMPLE_RATE = 48000
CHANNELS = 1
SAMPLE_WIDTH = 2  # 16-bit

# ADPCM tables (IMA)
ADPCM_INDEX_TABLE = [
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
]
ADPCM_STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
]


class AdpcmDecoder:
    """IMA ADPCM decoder matching the ESP32 implementation."""

    def __init__(self):
        self.predictor = 0
        self.index = 0

    def reset(self):
        self.predictor = 0
        self.index = 0

    def decode_frame(self, data: bytes) -> np.ndarray:
        """Decode ADPCM bytes to PCM int16 samples."""
        samples = []
        for byte in data:
            # Low nibble first, then high nibble
            for nibble_idx in range(2):
                if nibble_idx == 0:
                    nibble = byte & 0x0F
                else:
                    nibble = (byte >> 4) & 0x0F

                step = ADPCM_STEP_TABLE[self.index]
                diff = step >> 3

                if nibble & 4:
                    diff += step
                if nibble & 2:
                    diff += step >> 1
                if nibble & 1:
                    diff += step >> 2

                if nibble & 8:
                    self.predictor -= diff
                else:
                    self.predictor += diff

                # Clamp
                self.predictor = max(-32768, min(32767, self.predictor))

                self.index += ADPCM_INDEX_TABLE[nibble]
                self.index = max(0, min(88, self.index))

                samples.append(self.predictor)

        return np.array(samples, dtype=np.int16)


class AdpcmEncoder:
    """IMA ADPCM encoder matching the ESP32 implementation."""

    def __init__(self):
        self.predictor = 0
        self.index = 0

    def reset(self):
        self.predictor = 0
        self.index = 0

    def encode_frame(self, pcm: np.ndarray) -> bytes:
        """Encode PCM int16 samples to ADPCM bytes."""
        encoded = bytearray()
        nibble_buf = 0
        nibble_count = 0

        for sample in pcm:
            sample = int(sample)
            step = ADPCM_STEP_TABLE[self.index]
            diff = sample - self.predictor

            nibble = 0
            if diff < 0:
                nibble = 8
                diff = -diff

            if diff >= step:
                nibble |= 4
                diff -= step
            if diff >= (step >> 1):
                nibble |= 2
                diff -= (step >> 1)
            if diff >= (step >> 2):
                nibble |= 1

            # Reconstruct
            recon_diff = step >> 3
            if nibble & 4:
                recon_diff += step
            if nibble & 2:
                recon_diff += step >> 1
            if nibble & 1:
                recon_diff += step >> 2

            if nibble & 8:
                self.predictor -= recon_diff
            else:
                self.predictor += recon_diff
            self.predictor = max(-32768, min(32767, self.predictor))

            self.index += ADPCM_INDEX_TABLE[nibble]
            self.index = max(0, min(88, self.index))

            if nibble_count == 0:
                nibble_buf = nibble
                nibble_count = 1
            else:
                nibble_buf |= (nibble << 4)
                encoded.append(nibble_buf)
                nibble_count = 0

        return bytes(encoded)


class OpusCodecWrapper:
    """Wrapper for Opus encode/decode."""

    FRAME_SIZE = 960  # 20ms @ 48kHz

    def __init__(self):
        if HAS_OPUS:
            self.encoder = opuslib.Encoder(SAMPLE_RATE, CHANNELS, opuslib.APPLICATION_AUDIO)
            self.encoder.bitrate = 64000  # 64kbps
            self.encoder.complexity = 10  # Max quality (server has plenty of CPU)
            self.decoder = opuslib.Decoder(SAMPLE_RATE, CHANNELS)
        else:
            self.encoder = None
            self.decoder = None

    def decode_frame(self, data: bytes) -> np.ndarray:
        """Decode length-prefixed Opus frame to PCM."""
        if not self.decoder or len(data) < 2:
            return np.array([], dtype=np.int16)

        frame_len = struct.unpack('<H', data[:2])[0]
        if frame_len == 0 or frame_len > len(data) - 2:
            return np.array([], dtype=np.int16)

        opus_data = data[2:2 + frame_len]
        pcm_bytes = self.decoder.decode(opus_data, self.FRAME_SIZE)
        return np.frombuffer(pcm_bytes, dtype=np.int16)

    def encode_frame(self, pcm: np.ndarray) -> bytes:
        """Encode PCM to length-prefixed Opus frame."""
        if not self.encoder:
            return b''

        pcm_bytes = pcm.astype(np.int16).tobytes()
        opus_data = self.encoder.encode(pcm_bytes, self.FRAME_SIZE)

        # Length prefix (2 bytes LE)
        header = struct.pack('<H', len(opus_data))
        return header + opus_data


class PTalkSession:
    """Manages a single device session."""

    def __init__(self, codec_type: str = 'opus'):
        self.codec_type = codec_type
        self.state = 'IDLE'
        self.recorded_frames = []  # Raw encoded frames
        self.recorded_pcm = []     # Decoded PCM
        self.device_id = None
        self.session_count = 0

        if codec_type == 'opus':
            self.codec = OpusCodecWrapper()
        else:
            self.codec = type('AdpcmCodec', (), {
                'decoder': AdpcmDecoder(),
                'encoder': AdpcmEncoder(),
                'decode_frame': lambda self, d: self.decoder.decode_frame(d),
                'encode_frame': lambda self, p: self.encoder.encode_frame(p),
            })()

    def reset_session(self):
        self.recorded_frames = []
        self.recorded_pcm = []
        if hasattr(self.codec, 'decoder') and hasattr(self.codec.decoder, 'reset'):
            self.codec.decoder.reset()
        if hasattr(self.codec, 'encoder') and hasattr(self.codec.encoder, 'reset'):
            self.codec.encoder.reset()

    def receive_audio(self, data: bytes):
        """Process incoming encoded audio from device."""
        self.recorded_frames.append(data)

        # Decode for inspection/saving
        pcm = self.codec.decode_frame(data)
        if len(pcm) > 0:
            self.recorded_pcm.append(pcm)

    def save_recording(self) -> str:
        """Save recorded PCM to WAV file. Returns filename."""
        if not self.recorded_pcm:
            return ""

        all_pcm = np.concatenate(self.recorded_pcm)
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        filename = f"recordings/rec_{timestamp}_{self.session_count}.wav"
        os.makedirs("recordings", exist_ok=True)

        with wave.open(filename, 'w') as wf:
            wf.setnchannels(CHANNELS)
            wf.setsampwidth(SAMPLE_WIDTH)
            wf.setframerate(SAMPLE_RATE)
            wf.writeframes(all_pcm.tobytes())

        duration = len(all_pcm) / SAMPLE_RATE
        print(f"  [SAVE] {filename} ({duration:.1f}s, {len(all_pcm)} samples)")
        return filename

    def get_echo_data(self) -> list:
        """Get recorded frames for echo playback."""
        return self.recorded_frames.copy()


async def handle_device(websocket, args):
    """Handle a single WebSocket connection from a device."""
    remote = websocket.remote_address
    print(f"\n[CONNECT] Device connected from {remote}")

    session = PTalkSession(codec_type=args.codec)
    wav_data = None

    # Pre-load WAV file if specified
    if args.wav_file and os.path.exists(args.wav_file):
        wav_data = load_wav_as_encoded(args.wav_file)
        print(f"  [WAV] Loaded {args.wav_file} ({len(wav_data)} frames)")

    try:
        async for message in websocket:
            if isinstance(message, str):
                # Text message
                print(f"  [TEXT] <- {message}")

                # Handle handshake
                try:
                    data = json.loads(message)
                    if data.get('cmd') == 'device_handshake':
                        session.device_id = data.get('device_id', 'unknown')
                        print(f"  [HANDSHAKE] Device ID: {session.device_id}")
                        print(f"              FW: {data.get('firmware_version', '?')}")
                        continue
                except (json.JSONDecodeError, KeyError):
                    pass

                if message == "START":
                    session.state = 'LISTENING'
                    session.session_count += 1
                    session.reset_session()
                    print(f"  [STATE] LISTENING (session #{session.session_count})")

                elif message == "END":
                    if session.state == 'LISTENING':
                        session.state = 'PROCESSING'
                        print(f"  [STATE] PROCESSING ({len(session.recorded_frames)} frames received)")

                        # Save recording
                        saved = session.save_recording()

                        # Send PROCESSING
                        await websocket.send("PROCESSING")
                        print(f"  [TEXT] -> PROCESSING")
                        await asyncio.sleep(0.5)

                        # Prepare response audio (WAV file takes priority)
                        if wav_data:
                            response_frames = wav_data
                            source = f"wav({args.wav_file})"
                        elif args.echo:
                            response_frames = session.get_echo_data()
                            source = "echo"
                        else:
                            response_frames = session.get_echo_data()
                            source = "echo(default)"

                        if response_frames:
                            # Send SPEAKING
                            await websocket.send("SPEAKING")
                            print(f"  [TEXT] -> SPEAKING ({len(response_frames)} frames, {source})")

                            # Stream audio in batches to reduce WS overhead
                            # Batch 10 frames (~200ms audio) per WS message
                            # Send at ~1.05x real-time to build buffer gradually
                            BATCH_SIZE = 10
                            for i in range(0, len(response_frames), BATCH_SIZE):
                                batch = response_frames[i:i + BATCH_SIZE]
                                batch_data = b''.join(batch)
                                try:
                                    await websocket.send(batch_data)
                                except Exception:
                                    break
                                # 19ms/frame: slightly faster than 20ms real-time
                                await asyncio.sleep(0.019 * len(batch))
                                sent = i + len(batch)
                                if sent % 100 < BATCH_SIZE:
                                    print(f"  [STREAM] {sent}/{len(response_frames)} frames sent ({sent*0.02:.1f}s)", end='\r')
                            print(f"  [STREAM] Done: {len(response_frames)} frames sent ({len(response_frames)*0.02:.1f}s)    ")

                        # Send IDLE
                        await websocket.send("IDLE")
                        print(f"  [TEXT] -> IDLE")
                        session.state = 'IDLE'

            elif isinstance(message, bytes):
                # Binary message: may contain multiple length-prefixed Opus frames
                if session.state == 'LISTENING':
                    offset = 0
                    while offset + 2 <= len(message):
                        frame_len = struct.unpack('<H', message[offset:offset+2])[0]
                        if frame_len == 0 or offset + 2 + frame_len > len(message):
                            break
                        frame_data = message[offset:offset + 2 + frame_len]
                        session.receive_audio(frame_data)
                        offset += 2 + frame_len
                    frame_count = len(session.recorded_frames)
                    if frame_count % 50 == 0:
                        print(f"  [AUDIO] Received {frame_count} frames...", end='\r')

    except websockets.exceptions.ConnectionClosed as e:
        print(f"\n[DISCONNECT] {remote} (code={e.code})")
    except Exception as e:
        print(f"\n[ERROR] {remote}: {e}")

    print(f"[SESSION END] Total sessions: {session.session_count}")


def load_wav_as_encoded(wav_path: str) -> list:
    """Load a WAV file and encode it for streaming."""
    with wave.open(wav_path, 'r') as wf:
        sr = wf.getframerate()
        nch = wf.getnchannels()
        sw = wf.getsampwidth()
        raw = wf.readframes(wf.getnframes())

    # Convert to int16
    if sw == 2:
        data = np.frombuffer(raw, dtype=np.int16)
    elif sw == 3:
        # 24-bit: pad to 32-bit then shift
        samples = len(raw) // 3
        raw32 = bytearray(samples * 4)
        for i in range(samples):
            raw32[i*4+1:i*4+4] = raw[i*3:i*3+3]
        data = np.frombuffer(bytes(raw32), dtype=np.int32).astype(np.float64)
        data = (data / 2**16).astype(np.int16)
    else:
        data = np.frombuffer(raw, dtype=np.int16)

    # Ensure mono
    if nch > 1:
        data = data.reshape(-1, nch)[:, 0]

    # Resample if needed
    if sr != SAMPLE_RATE:
        ratio = SAMPLE_RATE / sr
        indices = np.arange(0, len(data), 1 / ratio).astype(int)
        indices = indices[indices < len(data)]
        data = data[indices]

    data = data.astype(np.int16)
    print(f"  [WAV] Loaded: {len(data)} samples, {len(data)/SAMPLE_RATE:.1f}s, resampled {sr}->{SAMPLE_RATE}Hz")

    # Encode in Opus frames
    frames = []
    codec = OpusCodecWrapper()
    frame_size = 960  # 20ms @ 48kHz
    for i in range(0, len(data) - frame_size + 1, frame_size):
        chunk = data[i:i + frame_size]
        encoded = codec.encode_frame(chunk)
        if encoded:
            frames.append(encoded)

    return frames


async def main(args):
    print("=" * 60)
    print("  PTalk V2 - Test Server")
    print("=" * 60)
    print(f"  WebSocket: ws://0.0.0.0:{args.port}/ws")
    print(f"  Codec:     {args.codec}")
    print(f"  Mode:      {'Echo' if args.echo else 'WAV: ' + str(args.wav_file) if args.wav_file else 'Echo (default)'}")
    print(f"  Recordings: ./recordings/")
    print("=" * 60)
    print("Waiting for device connections...\n")

    async with websockets.serve(
        lambda ws: handle_device(ws, args),
        "0.0.0.0",
        args.port,
        ping_interval=20,
        ping_timeout=30,
        max_size=2**20,  # 1MB max message
    ):
        await asyncio.Future()  # Run forever


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="PTalk V2 Test Server")
    parser.add_argument("--port", type=int, default=8000, help="WebSocket port (default: 8000)")
    parser.add_argument("--codec", default="opus",
                        help="Audio codec (opus)")
    parser.add_argument("--echo", action="store_true", default=True,
                        help="Echo recorded audio back (default: True)")
    parser.add_argument("--wav-file", type=str, default=None,
                        help="WAV file to send as TTS response instead of echo")
    args = parser.parse_args()

    os.chdir(os.path.dirname(os.path.abspath(__file__)))

    # Auto-detect WAV file in server directory if not specified
    if not args.wav_file:
        import glob
        wav_files = glob.glob("*.wav")
        if wav_files:
            args.wav_file = wav_files[0]
            print(f"[AUTO] Found WAV file: {args.wav_file}")

    asyncio.run(main(args))
