-- Read the retained FDN entry through physical Number list/Search/Details.
_G.npe3_fdn_record_steps={{42,'Scroll Down'},{43,'Scroll Down'},
    {46,'Left Softkey / Menu'},{48,'Left Softkey / Menu'},
    {50,'Left Softkey / Menu'},{53,'Left Softkey / Menu'}}
_G.npe3_fdn_record_finish=60
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_fdn_input.lua')
