-- Observe the post-acceptance UI through the same physical command contact.
_G.nsk3_followup_menu=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_security_input.lua')
local machine=manager.machine
local captured=false
emu.register_frame_done(function()
    if not captured and machine.time:as_double()>=13 then
        machine.screens[':screen']:snapshot('nsk3_before_menu.png')
        captured=true
    end
end)
