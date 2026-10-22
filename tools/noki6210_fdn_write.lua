-- Physical FDN Number list/Add with PIN2; card updates remain firmware-owned.
_G.npe3_fdn_record_steps={{42,'Scroll Down'},{43,'Scroll Down'},
    {46,'Left Softkey / Menu'},{48,'Scroll Down'},{50,'Left Softkey / Menu'},
    {53,'Keypad 5'},{54,'Keypad 6'},{55,'Keypad 7'},{56,'Keypad 8'},
    -- NPE-3 FDN Add asks for number before name.
    {57,'Left Softkey / Menu'},{60,'Keypad 1'},{61,'Keypad 2'},
    {62,'Keypad 3'},{63,'Keypad 4'},{64,'Keypad 5'},
    {65,'Left Softkey / Menu'},{68,'Keypad 2'},{71,'Left Softkey / Menu'}}
_G.npe3_fdn_record_finish=78
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_fdn_input.lua')
