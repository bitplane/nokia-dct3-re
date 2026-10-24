-- Restrict expensive serial-poll taps to the relevant six-second boot window.
_G.nse5_full_sample_times = {1,2,4,5}
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse5_fullbackup_v500_observe.lua')
