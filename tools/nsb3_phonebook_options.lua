-- Physical Names/Search/Details/Options on the retained laboratory contact.
_G.nsb3_phonebook_steps = {
    {34, 'Right Softkey / C'}, {37, 'Right Softkey / C'},
    {40, 'Left Softkey / Menu'}, {43, 'Left Softkey / Menu'},
    {46, 'Left Softkey / Menu'}, {49, 'Left Softkey / Menu'}}
for _, step in ipairs(_G.nsb3_phonebook_extra_steps or {}) do
    _G.nsb3_phonebook_steps[#_G.nsb3_phonebook_steps + 1] = step
end
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_phonebook_input.lua')
