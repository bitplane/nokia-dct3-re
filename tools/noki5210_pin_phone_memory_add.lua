_G.nsm5_phone_memory_mode = 'add'
local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
dofile(directory .. 'noki5210_pin_phone_memory.lua')
