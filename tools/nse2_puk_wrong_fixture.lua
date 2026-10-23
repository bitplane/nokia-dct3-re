_G.nse2_wrong_puk1=true
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_puk_fixture.lua')
