-- Compose physical PIN1, distinct phone code, then Menu in one cold boot.
local source=debug.getinfo(1,'S').source:sub(2)
local directory=source:match('^(.*[/\\])')
_G.nsm1_correct_pin=true
dofile(directory .. 'nsm1_pin_input.lua')
_G.nsm1_phone_code_start=15
dofile(directory .. 'nsm1_security_input.lua')
local machine=manager.machine
local captured=false
emu.register_frame_done(function()
    if not captured and machine.time:as_double()>=24 then
        machine.screens[':screen']:snapshot('nsm1_pin_menu.png')
        captured=true
    end
end)
