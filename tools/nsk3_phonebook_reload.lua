-- Read saved phonebook through physical UI contacts in a separate cold process.
_G.nsk3_followup_contacts={{1,16,0x1a},{1,2,0x19},{1,2,0x19},{1,2,0x19}}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_security_input.lua')
local machine=manager.machine
local next_frame=1
local frames={17,19}
emu.register_frame_done(function()
    if frames[next_frame] and machine.time:as_double()>=frames[next_frame] then
        machine.screens[':screen']:snapshot('nsk3_phonebook_reload_'..frames[next_frame]..'.png')
        next_frame=next_frame+1
    end
end)
