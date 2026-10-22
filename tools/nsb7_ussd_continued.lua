-- Dial, answer and reply through the actual handset UI.
_G.nsb7_ussd_snapshots={24}
_G.nsb7_ussd_actions={
    {26,'Keypad *'}, {27,'Keypad 1'}, {28,'Keypad 2'},
    {29,'Keypad 3'}, {30,'Keypad #'}, {32,'Send'},
    {36,'Left Softkey / Menu'}, {38,'Keypad 2'},
    {40,'Left Softkey / Menu'}, {44,'End'}, {46,'Left Softkey / Menu'},
}
_G.nsb7_ussd_finish=49
_G.nsb7_ussd_response_times={35,37,39,41,43,45,47}
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'nsb7_host_ussd_observe.lua')
