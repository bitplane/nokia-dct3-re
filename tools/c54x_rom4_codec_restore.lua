-- Passive save snapshots and a bounded replay of native DSP I/O writes.
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local io = assert(cpu.spaces['io'])
local data = assert(cpu.spaces['data'])
local saved
local replay = (tonumber(os.getenv('NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS')) or 0) / 1000
-- Leave margin before the harness loads again; scheduler settling belongs
-- outside the measured window, not inside an approximate event comparison.
local window = replay * 0.8
local anchor, phase, checked
local reference, restored_events = {}, {}
local overflow = {}
local writes = io:install_write_tap(0, 0xffff, 'codec_restore_io', function(address, value, mask)
    if not anchor or machine:side_effects_disabled() then return end
    local time = machine.time:as_double()
    if time >= anchor and time < anchor + window then
        local events = phase == 'reference' and reference or restored_events
        if #events >= 16384 then overflow[phase] = true; return end
        events[#events + 1] = string.format('%.9f:%04x:%04x:%04x', time, address, value, mask)
    end
end)
local function snapshot()
    return string.format('t=%.9f pc=%04x st0=%04x st1=%04x sp=%04x io21=%04x bspc22=%04x',
        machine.time:as_double(), cpu.state['PC'].value,
        cpu.state['ST0'].value, cpu.state['ST1'].value,
        cpu.state['SP'].value, io:read_u16(0x21), data:read_u16(0x22))
end
local before = emu.add_machine_pre_save_notifier(function()
    saved = snapshot()
    if replay > 0 then
        anchor, phase, checked = machine.time:as_double(), 'reference', false
        reference, restored_events = {}, {}
        overflow = {}
    end
    machine:logerror('rom4_codec_state: phase=saved ' .. saved .. '\n')
end)
local after = emu.add_machine_post_load_notifier(function()
    local restored = snapshot()
    machine:logerror('rom4_codec_state: phase=restored ' .. restored .. '\n')
    assert(saved and saved == restored, 'native codec state differs after load')
    if replay > 0 then phase = 'restored' end
end)
emu.register_frame_done(function()
    assert(writes)
    if not anchor or phase ~= 'restored' or checked or machine.time:as_double() < anchor + window then return end
    checked = true
    local passed = not overflow.reference and not overflow.restored and #reference > 0 and #reference == #restored_events
    for index, event in ipairs(reference) do
        if restored_events[index] ~= event then
            passed = false
            machine:logerror(string.format('rom4_codec_replay_mismatch: index=%d reference=%s restored=%s\n', index, event, restored_events[index] or 'missing'))
            break
        end
    end
    machine:logerror(string.format('rom4_codec_replay: result=%s reference_events=%d restored_events=%d window=%.9f\n', passed and 'pass' or 'fail', #reference, #restored_events, window))
end)
_G.rom4_codec_restore = {before, after, writes}
local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
dofile(directory .. '../mame_nokia_dct3_input_exerciser.lua')
