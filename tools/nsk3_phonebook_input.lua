-- Own raw-key table: test command1a after physical code acceptance.
_G.nsk3_followup_contacts={
    {1,16,0x1a},{1,8,0x18},{1,2,0x19}, -- Names, Down, Add entry
    {3,2,2},{1,2,0x19}, -- A, accept name
    {2,2,1},{3,2,2},{4,2,3},{2,4,4},{3,4,5},{1,2,0x19} -- 12345, Save
}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_security_input.lua')
local machine=manager.machine
local next_frame=1
local frames={17,23,25}
emu.register_frame_done(function()
    if frames[next_frame] and machine.time:as_double()>=frames[next_frame] then
        machine.screens[':screen']:snapshot('nsk3_phonebook_'..frames[next_frame]..'.png')
        next_frame=next_frame+1
    end
end)
