-- Extend the verified physical-menu workflow without changing device inputs.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. (_G.nsk3_long_input_script or 'nsk3_menu_input.lua'))
local machine=manager.machine
local next_frame=1
local frames={30,60,90,110}
emu.register_frame_done(function()
    if frames[next_frame] and machine.time:as_double()>=frames[next_frame] then
        machine.screens[':screen']:snapshot('nsk3_long_'..frames[next_frame]..'.png')
        next_frame=next_frame+1
    end
end)
