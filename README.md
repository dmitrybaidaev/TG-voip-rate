# TG-voip-rate

Console application estimating quality degradation of speech signal, uses PESQ MOS.
Building: use build_linux.sh 

Usage: tgvoiprate 1_mono_16khz.wav 2_mono_16khz.wav
Result: number in range [1-5], where 5 - no degradation, 1 - fully degraded signal
