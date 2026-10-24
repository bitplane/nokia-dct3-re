-- Physical Options/Edit on the persisted laboratory SIM contact.
_G.nsb3_phonebook_extra_steps = {
    {52, 'Left Softkey / Menu'}, {55, 'Right Softkey / C'},
    {58, 'Keypad 3'}, {61, 'Left Softkey / Menu'},
    {64, 'Right Softkey / C'}, {67, 'Keypad 6'}, {70, 'Left Softkey / Menu'},
    {73, 'Left Softkey / Menu'}}
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_phonebook_options.lua')
