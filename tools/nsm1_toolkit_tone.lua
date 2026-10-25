-- Own physical Quit and bounded native state; no DSP or guest publication.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsm1_toolkit_menu.lua')
local machine=manager.machine
local dsp=assert(machine.devices[':dsp_c54x:cpu'])
local until_time=dofile(directory..'fixture_deadline.lua')
local function snapshot(label)
    -- "stop" reads ILLEGAL; avoid looking like an execution-error diagnostic.
    machine:logerror(string.format('nsm1_toolkit_tone_native: phase=%s pc=%04x idle=%d stop=%d t=%.9f observer_writes=0\n',
        label,dsp.state['PC'].value,dsp.state['IDLE'].value,dsp.state['ILLEGAL'].value,machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsm1_toolkit_tone_'..label..'.png')
end
local actions=coroutine.create(function()
    until_time(85)
    snapshot('before')
    until_time(87)
    snapshot('active')
    if _G.nsm1_toolkit_tone_cancel~=false then
        until_time(89)
        local quit
        for _,port in pairs(machine.ioport.ports) do
            for _,field in pairs(port.fields) do
                if field.name=='Right Softkey' then quit=field end
            end
        end
        assert(quit,'own physical Quit contact missing')
        quit:set_value(1)
        machine:logerror('nsm1_toolkit_tone: action=quit down=1\n')
        until_time(89.15)
        quit:set_value(0)
        machine:logerror('nsm1_toolkit_tone: action=quit down=0\n')
    end
    until_time(91)
    snapshot('after')
end)
assert(coroutine.resume(actions))
