-- Read saved card through physical Names/Search/List/Details after cold boot.
_G.nsm1_followup_contacts={{1,16,0x1a},{1,2,0x19},{1,2,0x19},{1,2,0x19}}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_security_input.lua')
local machine=manager.machine
local times={16,17,18,20,22}
local index=1
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot(string.format('nsm1_phonebook_read_%d.png',times[index]))
        index=index+1
    end
end)
