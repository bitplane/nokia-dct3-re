-- Cold authentication and passive idle samples; no display-selector changes.
local directory = debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
_G.nsk3_sim_tx_limit = 4096
dofile(directory .. 'nsk3_pin_reload.lua')
_G.nsk3_phone_code_start = 15
dofile(directory .. 'nsk3_security_input.lua')
local machine = manager.machine
local dsp = assert(machine.devices[':dsp_c54x:cpu'])
local samples = coroutine.create(function()
    for index, time in ipairs({25,29,31,36,42}) do
        assert(emu.wait(time-machine.time:as_double()))
        machine.screens[':screen']:snapshot(string.format('nsk3_idle_%02d.png',time))
        machine:logerror(string.format('nsk3_diagnostic_native: phase=cold sample=%d pc=%06x stopped=%d idle=%d observer_writes=0\n',
            index,dsp.state.PC.value,dsp.state.ILLEGAL.value,dsp.state.IDLE.value))
    end
end)
assert(coroutine.resume(samples))
