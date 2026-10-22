-- Dial *123# through the real keypad; the host answers the organic request.
_G.nsb7_ussd_snapshots={24}
_G.nsb7_ussd_actions={
    {26,'Keypad *'}, {27,'Keypad 1'}, {28,'Keypad 2'},
    {29,'Keypad 3'}, {30,'Keypad #'}, {32,'Send'},
    {38,'End'}, {40,'Left Softkey / Menu'},
}
_G.nsb7_ussd_finish=43
_G.nsb7_ussd_response_times={36,37}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_host_ussd_observe.lua')
