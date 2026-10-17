-- Actual data-bus observations, not retained CPU operands or retirement proof.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'polynomial observation requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = assert(cpu.spaces.data)
local handles, counts = {}, {}
local closed = false
local function observe(kind, address, value)
    local time, pc = machine.time:as_double(), cpu.state.GENPC.value
    if closed or machine:side_effects_disabled() or time < 0.9 or time > 1.2 or
       pc < 0x260a0 or pc > 0x260d0 or (counts[kind] or 0) >= 64 then return end
    counts[kind] = (counts[kind] or 0) + 1
    machine:logerror(string.format(
        'mu4_playback_poly_bus: kind=%s address=%04x value=%04x ordinal=%u st0=%04x st1=%04x pmst=%04x t=%.9f context_pc=%06x boundary=data_bus ownership=unvalidated observer_writes=0\n',
        kind, address, value, counts[kind], cpu.state.ST0.value, cpu.state.ST1.value,
        cpu.state.PMST.value, time, pc))
end
handles[1] = data:install_read_tap(0xc22a, 0xc22f, 'poly_coefficients',
    function(address, value) observe('read', address, value) end)
handles[2] = data:install_read_tap(0x1170, 0x1170, 'poly_t_source',
    function(address, value) observe('read', address, value) end)
handles[3] = data:install_write_tap(0x1174, 0x1175, 'poly_result',
    function(address, value) observe('write', address, value) end)
emu.register_frame_done(function()
    if not closed and machine.time:as_double() > 1.2 then
        for _, handle in ipairs(handles) do handle:remove() end
        closed = true
        machine:logerror('mu4_playback_poly_taps_closed: observer_writes=0\n')
    end
end)
