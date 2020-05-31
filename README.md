# Supersaw
Korg Prologue Custom OSC to offer 12xSaw/Square wave oscillator + noise

controls:
shape knob - SAW <-> SQR

edit section:
VOICES - number of voices to use, this is per actual voice of multi engine
DETUNE - amount of detune per voice, this is calculated as an even detune per voice instance
NOISE LVL - noise mix level, higher the noise will result in lower volume of the saw wave mix (calculated as noise * noise lvl + (1 - noise level) * saw)
PHASE SYNC - whether to restart phases of all voices upon retrigger (1 = off, 2 = on)
COMP THR - threshold for the internal compressor for adjusting volume, 100% = uncompressed, 0% = fully compressed.  ratio internally is 0.2  (1/5)
