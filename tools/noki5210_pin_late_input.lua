local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki5210_pin_start_delay = 12
_G.noki5210_pin_gap = 1.85
dofile(directory .. 'noki5210_measurement_observe.lua')
dofile(directory .. 'noki5210_pin_input.lua')
