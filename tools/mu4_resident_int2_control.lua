_G.mu4_int2_fixture_assert=false
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'mu4_resident_int2_observe.lua')
