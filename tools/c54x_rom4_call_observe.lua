-- Passive native NSE-1 call-attempt observation; no network/speech claim.
local machine = manager.machine
local dsp = assert(machine.devices[':dsp_c54x:cpu'])
local mcu = assert(machine.devices[':maincpu'])
assert(dsp.debug, 'call observer requires -debug -debugger none')
machine.debugger.visible_cpu = dsp
machine.debugger:command('temp0=0;temp1=0')
assert(dsp.spaces.program:read_u16(0x9c4f) == 0xf074)
assert(dsp.spaces.program:read_u16(0x9cb9) == 0xf074)
for index, address in ipairs({0x9c4f, 0x9cb9}) do
    local counter = 'temp' .. (index - 1)
    dsp.debug:bpset(address, nil, counter .. '=' .. counter .. '+1;g')
end
local taps, counts = {}, {}
local function record(kind, address, value, mask)
    local time = machine.time:as_double()
    if time < 7 then return end
    counts[kind] = (counts[kind] or 0) + 1
    if counts[kind] <= 512 then
        machine:logerror(string.format(
            'rom4_call_bus: kind=%s address=%08x value=%08x mask=%08x t=%.9f\n',
            kind, address, value, mask, time))
    end
end
taps[#taps + 1] = mcu.spaces.program:install_write_tap(
    0x10000, 0x10fff, 'call_host_write', function(address, value, mask)
        record('host_write', address, value, mask)
    end)
taps[#taps + 1] = dsp.spaces.data:install_read_tap(
    0x0854, 0x0854, 'call_payload_read', function(address, value, mask)
        record('payload_read', address, value, mask)
    end)
taps[#taps + 1] = dsp.spaces.io:install_write_tap(
    0x2c, 0x2d, 'call_cobba_write', function(address, value, mask)
        record('cobba_write', address, value, mask)
    end)
local stop = emu.add_machine_stop_notifier(function()
    machine.debugger:command('logerror "rom4_call_codec: encoder=%d decoder=%d\\n",temp0,temp1')
    machine:logerror(string.format(
        'rom4_call_end: machine=%s illegal=%d mode=%04x flags=%04x\n',
        machine.system.name, dsp.state.ILLEGAL.value,
        dsp.spaces.data:read_u16(0x1836), dsp.spaces.data:read_u16(0x06bb)))
    for _, kind in ipairs({'host_write', 'payload_read', 'cobba_write'}) do
        machine:logerror(string.format('rom4_call_count: kind=%s count=%d cap=512\n',
            kind, counts[kind] or 0))
    end
end)
_G.rom4_call_observer = {taps, counts, stop}
local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
dofile(directory .. '../mame_nokia_dct3_input_exerciser.lua')
machine.debugger.execution_state = 'run'
