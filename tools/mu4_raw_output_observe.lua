-- Passive call-2 buffer chronology. GENPC is context, not instruction ownership.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'raw output observation requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local handles, count, closed = {}, 0, false
local function record(kind, address, value, mask)
    if machine:side_effects_disabled() then return end
    local time = machine.time:as_double()
    if time < 1.0168 or time > 1.023 then return end
    count = count + 1
    if count > 1024 then return end
    machine:logerror(string.format('mu4_raw_%s: address=%04x value=%04x mask=%04x context_pc=%06x a=%010x b=%010x ar1=%04x ar2=%04x t=%.9f ownership=data_bus observer_writes=0\n',
        kind, address, value, mask, cpu.state.GENPC.value, cpu.state.A.value,
        cpu.state.B.value, cpu.state.AR1.value, cpu.state.AR2.value, time))
end
for _, base in ipairs({0xf380, 0xeb80, 0x7b80, 0xf300, 0x7800, 0xe800, 0x787e, 0xe87e}) do
    handles[#handles + 1] = cpu.spaces.data:install_write_tap(base, base + 1, 'raw_writer_' .. base,
        function(address, value, mask) record('writer', address, value, mask) end)
end
for _, base in ipairs({0x7b80, 0xf300, 0x7800, 0xe800, 0x787e, 0xe87e}) do
    handles[#handles + 1] = cpu.spaces.data:install_read_tap(base, base + 1, 'raw_reader_' .. base,
        function(address, value, mask) record('reader', address, value, mask) end)
end
emu.register_frame_done(function()
    if not closed and machine.time:as_double() > 1.023 then
        for _, handle in ipairs(handles) do handle:remove() end
        closed = true
    end
end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('mu4_raw_terminal: records=%u closed=%u capped=%u observer_writes=0 audio_acceptance=0\n',
        count, closed and 1 or 0, count > 1024 and 1 or 0))
end)
_G.mu4_raw_output_handles = {handles, stop}
