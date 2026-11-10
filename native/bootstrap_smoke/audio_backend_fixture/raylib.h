#pragma once
// Synthetic test API surface only. The real runner compiles against pinned raylib.
struct AudioStream {void* buffer;unsigned sampleRate,sampleSize,channels;};
using AudioCallback=void(*)(void*,unsigned);
bool IsAudioDeviceReady();
AudioStream LoadAudioStream(unsigned,unsigned,unsigned);
bool IsAudioStreamValid(AudioStream);
void UnloadAudioStream(AudioStream);
void SetAudioStreamCallback(AudioStream,AudioCallback);
void PlayAudioStream(AudioStream);
void StopAudioStream(AudioStream);
bool IsAudioStreamPlaying(AudioStream);
