-- The cold mixed list selects SIM A first; move explicitly to Phone D.
_G.nsm1_followup_contacts = {{1,16,0x1a}, {1,2,0x19}, {1,2,0x19}, {1,8,0x18}, {1,2,0x19}}
local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
dofile(directory .. 'nsm1_security_input.lua')
local machine = manager.machine
local times, index = {17,18,20,22}, 1
emu.register_frame_done(function()
    if times[index] and machine.time:as_double() >= times[index] then
        machine.screens[':screen']:snapshot('nsm1_phone_local_read_' .. times[index] .. '.png')
        index = index + 1
    end
end)
