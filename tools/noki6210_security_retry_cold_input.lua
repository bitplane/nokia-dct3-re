-- First process stops with the card's real failed-PIN retry count retained.
_G.npe3_pin_retry_wrong_only=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_security_retry_input.lua')
