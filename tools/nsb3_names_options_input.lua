-- Enumerate the own physical Names/Options menu, then inspect Memory status.
_G.nsb3_phonebook_steps = {
    {34, 'Right Softkey / C'}, {37, 'Right Softkey / C'},
    {40, 'Scroll Down'}, {43, 'Scroll Down'}, {46, 'Scroll Down'},
    {49, 'Left Softkey / Menu'}, {52, 'Scroll Down'},
    {55, 'Scroll Down'}, {58, 'Scroll Down'}, {61, 'Left Softkey / Menu'}}
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_phonebook_input.lua')
