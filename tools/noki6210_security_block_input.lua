-- Reach the blocked card through three real failed PIN entries.
_G.npe3_pin_retry_wrong_only=true
_G.npe3_pin_retry_attempts=3
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_security_retry_input.lua')
