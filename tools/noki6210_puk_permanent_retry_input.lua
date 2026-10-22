-- Attempt the correct PUK physically after all ten PUK retries are exhausted.
_G.npe3_puk_permanent_retry=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_puk_input.lua')
