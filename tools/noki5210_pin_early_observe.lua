local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
dofile(directory .. 'noki5210_measurement_observe.lua')
dofile(directory .. 'noki5210_pin_input.lua')
