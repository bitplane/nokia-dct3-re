_G.nse2_pin2_wrong=true
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_pin2_fixture.lua')
