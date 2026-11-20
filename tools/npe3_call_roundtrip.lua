-- 6210 ROM6 lab early incoming call (page 18 s, Send 20 s, End 30 s) with a
-- save/replay/load round trip at NOKIA_DCT3_STATE_ROUNDTRIP_AT. No research
-- observers: their Lua counters are outside the machine image.
-- The machine saves, runs NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS recording every
-- DSP I/O write, loads, and must repeat them (c54x_rom4_codec_restore.lua).
-- Loading cancels every pending emu.wait, so the restored branch schedules
-- the inputs still due itself, at the ordinary run's times.
local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
_G.codec_restore_without_exerciser = true
dofile(directory .. 'c54x_rom4_codec_restore.lua')
local machine = manager.machine
local dsp = assert(machine.devices[':dsp_c54x:cpu'])
local mcu = assert(machine.devices[':maincpu']).spaces['program']
local at = assert(tonumber(os.getenv('NOKIA_DCT3_STATE_ROUNDTRIP_AT')), 'round trip time')
local replay = assert(tonumber(os.getenv('NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS'))) / 1000
local events = {{18, ':LAB_BTS', 'Laboratory BTS page', 0.05}, {20, ':COL.0', 'Send', 0.15},
                {30, ':COL.0', 'End', 0.15}}
for _, event in ipairs(events) do
    assert(at < event[1] - replay - 0.01 or at > event[1] + event[4], 'round trip overlaps an input')
end
local function inputs()
    local task = coroutine.create(function()
        for _, event in ipairs(events) do
            local now = machine.time:as_double()
            if event[1] >= now then
                if not emu.wait(event[1] - now) then return end
                local field = assert(machine.ioport.ports[event[2]].fields[event[3]])
                machine:logerror(string.format('npe3_call_input: key=%s t=%.6f\n', event[3], machine.time:as_double()))
                field:set_value(1)
                if not emu.wait(event[4]) then field:set_value(0); return end
                field:set_value(0)
            end
        end
    end)
    assert(coroutine.resume(task))
    return task
end
_G.npe3_call_inputs = inputs()
local function structure()
    return string.format('fiq=%02x irq=%02x ctrl=%02x dsp_idle=%d dsp_illegal=%d',
        mcu:read_u8(0x2000a), mcu:read_u8(0x2000b), mcu:read_u8(0x2000c),
        dsp.state['IDLE'].value, dsp.state['ILLEGAL'].value)
end
_G.npe3_call_roundtrip = coroutine.create(function()
    if not emu.wait(at) then return end
    local saved = structure()
    local requested = machine.time:as_double()
    machine:logerror(string.format('state_replay: phase=reference event=begin t=%.6f\n', requested))
    machine:save('npe3_lab_call')
    if not emu.wait(replay) then return end
    machine:logerror(string.format('state_replay: phase=reference event=end t=%.6f\n', machine.time:as_double()))
    machine:load('npe3_lab_call')
    emu.wait(0.01) -- resumed (cancelled) by the load itself
    local restored = structure()
    machine:logerror(string.format('state_roundtrip: result=%s scenario=npe3_lab_call %s requested_at=%.6f t=%.6f\n',
        (restored == saved and dsp.state['ILLEGAL'].value == 0) and 'pass' or 'fail', restored, requested,
        machine.time:as_double()))
    _G.npe3_call_inputs = inputs()
end)
assert(coroutine.resume(_G.npe3_call_roundtrip))
