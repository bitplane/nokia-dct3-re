-- Own physical dial, continued reply and ordinary idle/Menu recovery.
_G.npe3_host_ussd_capture='6210_ussd_continued_before.png'
_G.npe3_host_ussd_actions={
    {26,'Keypad *'},{27,'Keypad 1'},{28,'Keypad 2'},
    {29,'Keypad 3'},{30,'Keypad #'},{31,'Send'},
    {36,'Left Softkey / Menu'},{40,'Keypad 2'},
    {42,'Left Softkey / Menu'},{46,'End'},{48,'Left Softkey / Menu'}}
_G.npe3_host_ussd_finish=51
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_host_ussd_notify.lua')
