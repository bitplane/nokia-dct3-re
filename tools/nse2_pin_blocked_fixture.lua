-- Cold blocked-card notice precedes the persistent PUK editor.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_boot_observe.lua')
local machine=manager.machine
local captured=false
emu.register_frame_done(function()
    if captured or machine.time:as_double()<11 then return end
    captured=true
    machine.screens[':screen']:snapshot('nse2_pin_blocked.png')
end,'NSE-2 cold PUK prompt observation')
