-- Physical PIN2-protected Erase; cancellation uses the actual Back contact.
_G.nsb7_fdn_record_steps={{59,'Left Softkey / Menu'},
    {62,'Keypad 5'},{63,'Keypad 6'},{64,'Keypad 7'},{65,'Keypad 8'},
    {66,'Left Softkey / Menu'},{69,'Scroll Down'},{72,'Left Softkey / Menu'},
    {75,_G.nsb7_fdn_cancel and 'Right Softkey / C' or 'Left Softkey / Menu'}}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_fdn_read.lua')
