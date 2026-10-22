-- Own physical erase confirmation, with a separate cancellation fixture.
_G.nhm3_fdn_record_steps={
    {42,'Scroll Down'},{43,'Scroll Down'},{46,'Left Softkey / Menu'},
    {48,'Left Softkey / Menu'},{50,'Scroll Down'},{51,'Scroll Down'},
    {52,'Scroll Down'},{55,'Left Softkey / Menu'},
    {58,'Keypad 5'},{59,'Keypad 6'},{60,'Keypad 7'},{61,'Keypad 8'},
    {62,'Left Softkey / Menu'},
    {65,_G.nhm3_fdn_cancel and 'Right Softkey / C' or 'Left Softkey / Menu'},
}
_G.nhm3_fdn_record_finish=72
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_fdn.lua')
