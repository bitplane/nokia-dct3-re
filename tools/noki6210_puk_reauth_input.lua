-- Cold authentication with the PIN installed by the physical PUK flow.
_G.npe3_puk_verify_rotated=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_puk_input.lua')
