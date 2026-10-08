local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'noki6250_runtime_observe.lua')
dofile(directory .. 'noki6250_slow_pin_input.lua')
