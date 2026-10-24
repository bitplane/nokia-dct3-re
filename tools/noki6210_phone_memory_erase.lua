_G.npe3_phone_memory_action='erase'
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_phone_memory_lifecycle.lua')
