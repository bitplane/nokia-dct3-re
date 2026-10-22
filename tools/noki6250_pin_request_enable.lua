_G.nhm3_pin_request_enable=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_pin_request.lua')
