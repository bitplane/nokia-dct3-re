-- Physical Names/Add entry, A/12345. Menu details belong only to the harness.
_G.nsm1_followup_contacts={
    {1,16,0x1a},{1,8,0x18},{1,2,0x19},
    {3,2,2},{1,2,0x19},
    {2,2,1},{3,2,2},{4,2,3},{2,4,4},{3,4,5},{1,2,0x19}
}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_security_input.lua')
local machine=manager.machine
local times={17,20,23,25,27,29}
local index=1
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot(string.format('nsm1_phonebook_%d.png',times[index]))
        index=index+1
    end
end)
