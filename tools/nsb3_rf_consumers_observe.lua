-- Physical startup plus passive DSP RF attribution; no sample source attached.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = source:match('^(.*[/\\])')
dofile(directory .. 'nsb3_pin_correct.lua')
dofile(directory .. 'c54x_rom4_rf_consumers_observe.lua')
