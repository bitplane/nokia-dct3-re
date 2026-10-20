-- Read-only NSK-1 startup instrument. No keys, RAM writes or peer replies.
local machine = manager.machine
assert(machine.system.name == 'nsk1r4t')
local cpu = machine.devices[':maincpu']
local space = cpu.spaces.program
local dsp = machine.devices[':dsp_c54x:cpu']
local releases, released, shared_writes = 0, false, 0
_G.nsk1_release_tap = space:install_write_tap(0x20000, 0x20003, 'nsk1_release', function(offset, data, mask)
    if (mask & 0xff00) == 0 then return end
    local command = (data >> 8) & 255
    if (command & 1) == 0 then released = false; return end
    if released then return end
    released = true
    releases = releases + 1
    if releases > 8 then return end
    local arguments, payload = {}, {}
    for index = 0, 6 do arguments[#arguments+1] = string.format('%04x', space:read_u16(0x100f6+index*2)) end
    for index = 0, 185 do payload[#payload+1] = string.format('%04x', space:read_u16(0x11e00+index*2)) end
    machine:logerror(string.format('nsk1_release: index=%d command=%02x pc=%08x args=%s program=%s t=%.9f observer_writes=0\n',
        releases, command, cpu.state['R15'].value, table.concat(arguments), table.concat(payload), machine.time:as_double()))
end)
_G.nsk1_shared_tap = space:install_write_tap(0x10000, 0x10fff, 'nsk1_shared', function(offset, data, mask)
    shared_writes = shared_writes + 1
end)
local times, next_sample = {0.1, 0.5, 1, 2, 5}, 1
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if next_sample > #times or now < times[next_sample] then return end
    machine:logerror(string.format('nsk1_boot: t=%.9f pc=%08x sp=%08x dsp_pc=%04x illegal=%d idle=%d result=%04x/%04x/%04x/%04x catalogue25=%08x shared_writes=%d releases=%d observer_writes=0\n',
        now, cpu.state['R15'].value, cpu.state['R13'].value,
        dsp.state['PC'].value, dsp.state['ILLEGAL'].value, dsp.state['IDLE'].value,
        space:read_u16(0x10000), space:read_u16(0x10002), space:read_u16(0x10004), space:read_u16(0x10006),
        space:read_u32(0x10b2c4), shared_writes, releases))
    if next_sample == #times then machine.screens[':screen']:snapshot('nsk1_native_5s.png') end
    next_sample = next_sample+1
end, 'NSK-1 passive native boundary')
