/*
    BSD 3-Clause License

    Copyright (c) 2018, KORG INC.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    * Neither the name of the copyright holder nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

//*/

/*
 * File: supersaw.cpp
 *
 * Naive sine oscillator test
 *
 */

#include "userosc.h"

typedef struct State {
  float drive;
  float lfo, lfoz;
  uint8_t flags;
  int32_t voices;
  int32_t detune;
  float noiseLevel;
  int8_t waveform;
  int8_t phaseSync;
  float sideLevel;
} State;

static State s_state;

struct instance {
	float phase;
	float w0;
	float level;
};

int maxVoice = 12;
struct instance instances[12];

enum {
  k_flags_none = 0,
  k_flag_reset = 1<<0,
};

void OSC_INIT(uint32_t platform, uint32_t api)
{
  s_state.drive = 1.f;
  s_state.lfo = s_state.lfoz = 0.f;
  s_state.flags = k_flags_none;
  s_state.voices = 2;
  s_state.detune = 10;
  s_state.noiseLevel = 0.f;
  s_state.waveform = 0;
}

__fast_inline uint16_t adjustPitch(uint16_t note, int32_t fineAdjust) {
	// high byte = note number, low byte = fine
	// mod 0 - 255 only adds to original pitch, negative mod applied as note - 1 + (255 + mod)
	uint8_t pitch = note >> 8; 
	uint8_t mod = note & 0xFF;
	uint16_t res = 0;

	// apply fine adjustment
	int32_t pitchChange = (fineAdjust + mod) / 255;
	int32_t adjustedMod = (fineAdjust + mod) % 255;
	if (adjustedMod < 0) {
		pitchChange--;
		adjustedMod += 0xFF;
	} 
	pitch += pitchChange;
	mod = adjustedMod;

	res = pitch;
	res = res << 8;
	res |= mod;
	return res;
}

__fast_inline float generate(int8_t waveform, float phase) {
	if (waveform == 0) {
		return (2.0f - 2 * phase) - 1.0f;
	}
	return phase > 0.5f ? -1.0f : 1.0f;
}

void OSC_CYCLE(const user_osc_param_t * const params,
               int32_t *yn,
               const uint32_t frames)
{  
  const uint8_t flags = s_state.flags;
  s_state.flags = k_flags_none;
  
  const float drive = s_state.drive;

  const float lfo = s_state.lfo = q31_to_f32(params->shape_lfo);
  float lfoz = (flags & k_flag_reset) ? lfo : s_state.lfoz;
  const float lfo_inc = (lfo - lfoz) / frames;
  
  q31_t * __restrict y = (q31_t *)yn;
  const q31_t * y_e = y + frames;

  uint8_t voices = s_state.voices;
  uint8_t detune = s_state.detune;

  // reset instances phase increment and phase
  int i;
  for (i = 0; i < voices && i < maxVoice; i++) {
		int32_t fineAdj = (i - voices >> 1) * detune;
		uint16_t voicePitch = adjustPitch(params->pitch, fineAdj);
		instances[i].w0 = osc_w0f_for_note(voicePitch >> 8, voicePitch & 0xFF);
		if (flags & k_flag_reset) {
			instances[i].phase = s_state.phaseSync ? 0.f : osc_white();
		}
  }
  
  // write to buffer
  for (; y != y_e; ) {
	float totalSig = 0.0f;
	for (i = 0; i < voices && i < maxVoice; i++) {
		float phase = instances[i].phase;
		totalSig += generate(s_state.waveform, phase) * instances[i].level * (1.0f / (float)voices);
		phase += instances[i].w0;
		phase -= (uint32_t) phase;
		instances[i].phase = phase;
	}

	totalSig = totalSig * (1.f - s_state.noiseLevel) + osc_white() * s_state.noiseLevel;
	const float sig  = osc_softclipf(0.05f, drive * totalSig);
	*(y++) = f32_to_q31(sig);
    lfoz += lfo_inc;
  }
  
  s_state.lfoz = lfoz;
}

void OSC_NOTEON(const user_osc_param_t * const params)
{
  s_state.flags |= k_flag_reset;
}

void OSC_NOTEOFF(const user_osc_param_t * const params)
{
  (void)params;
}

void updateVoiceLevels() {
	int i;
	float voice_count = s_state.voices;
	float mid = voice_count / 2.f;
	for (i = 0; i < voice_count && i < maxVoice; i++) {
		instances[i].level = (i > mid) ? (float) (mid - i) : (float) (i - mid);
		instances[i].level = (instances[i].level + mid) / mid;
		instances[i].level = instances[i].level * s_state.sideLevel + (1.f - s_state.sideLevel);
	}
}

void OSC_PARAM(uint16_t index, uint16_t value)
{
  const float valf = param_val_to_f32(value);
  
  switch (index) {
  case k_user_osc_param_id1:
	 s_state.voices = value + 1;
	 updateVoiceLevels();
	 break;
  case k_user_osc_param_id2:
	 s_state.detune = value + 1;
	 break;
  case k_user_osc_param_id3:
	 s_state.noiseLevel = value / 100.f;
	 break;
  case k_user_osc_param_id4:
	 s_state.waveform = value;
	 break;
  case k_user_osc_param_id5:
	 s_state.phaseSync = value;
	 break;
  case k_user_osc_param_id6:
	 s_state.sideLevel = value / 100.f;
	 updateVoiceLevels();
	 break;
  default:
    break;
  }
}

