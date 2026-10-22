_G.nhm3_fdn_record_steps={
    {42,'Scroll Down'},{43,'Scroll Down'},{46,'Left Softkey / Menu'},
}
_G.nhm3_fdn_record_finish=54
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_fdn.lua')
