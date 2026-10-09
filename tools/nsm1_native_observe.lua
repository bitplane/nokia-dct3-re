-- Read-only NSM-1 compatibility observer; no synthetic DSP publications.
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces['program']
local writes = 0
local handles = {}
handles[#handles + 1] = memory:install_write_tap(0x10000, 0x10103,
    'nsm1_native_shared', function(offset, value, mask)
        writes = writes + 1
        if writes <= 64 then
            machine:logerror(string.format(
                'nsm1_native_shared: address=%08x value=%08x mask=%08x pc=%08x t=%.9f\n',
                offset, value, mask, cpu.state['PC'].value,
                machine.time:as_double()))
        end
    end)
local next_sample = 0
local captured = false
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if now < next_sample then return end
    next_sample = next_sample + 1
    machine:logerror(string.format(
        'nsm1_native_snapshot: pc=%08x sp=%08x lr=%08x shared_writes=%u t=%.9f\n',
        cpu.state['PC'].value, cpu.state['R13'].value,
        cpu.state['R14'].value, writes, now))
    if now >= 8 and not captured then
        captured = true
        machine:logerror(string.format(
            'nsm1_native_result: verifier_first=%04x verifier_second=%04x t=%.9f\n',
            memory:read_u16(0x111a9e), memory:read_u16(0x111aa0), now))
        machine.screens[':screen']:snapshot('6150-native-compatibility.png')
    end
end, 'frame')
_G.nsm1_native_observer_handles = handles
