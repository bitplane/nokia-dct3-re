-- Edit asks for name before number, unlike the NPE-3 Add editor.
_G.npe3_fdn_record_steps={{42,'Scroll Down'},{43,'Scroll Down'},
    {46,'Left Softkey / Menu'},{48,'Left Softkey / Menu'},
    {50,'Scroll Down'},{51,'Scroll Down'},{54,'Left Softkey / Menu'},
    {57,'Keypad 5'},{58,'Keypad 6'},{59,'Keypad 7'},{60,'Keypad 8'},
    {61,'Left Softkey / Menu'},{64,'Left Softkey / Menu'},
    {67,'Right Softkey / C'},{70,'Keypad 6'},{73,'Left Softkey / Menu'}}
_G.npe3_fdn_record_finish=80
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_fdn_input.lua')
