-- No-input cold observation: enabled PIN and separate phone-lock prompts.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_boot_observe.lua')
local machine,index=manager.machine,1
local frames={10,20,30,40}
emu.register_frame_done(function()
    if frames[index] and machine.time:as_double()>=frames[index] then
        machine.screens[':screen']:snapshot('nsk1_pin_request_cold_'..index..'.png')
        index=index+1
    end
end)
