-- Address-neutral bounded observation of whole NSE-5 backup inputs.
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
local dsp = assert(machine.devices[':dsp_c54x:cpu'])
local index = 1
local times = _G.nse5_full_sample_times or {1,2,4,8,16,24,36,48}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if times[index] and now >= times[index] then
        machine.screens[':screen']:snapshot(string.format('nse5_full_%02d.png', times[index]))
        machine:logerror(string.format('nse5_full_sample: index=%d t=%.9f arm=%08x sp=%08x dsp=%06x stopped=%d idle=%d observer_writes=0\n',
            index, now, cpu.state.R15.value, cpu.state.R13.value,
            dsp.state.PC.value, dsp.state.ILLEGAL.value, dsp.state.IDLE.value))
        index = index+1
    end
end)
