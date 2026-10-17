-- Passive AA88 caller frames selected by argument reads; no guest writes.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'recorder header observation requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = assert(cpu.spaces.data)
local sampling, closed = false, false
local handles, counts = {}, {}
handles[1] = data:install_read_tap(0x6000, 0x7fff, 'mu4_recorder_header_arguments', function(address, value)
    local pc, time = cpu.state.GENPC.value, machine.time:as_double()
    if sampling or machine:side_effects_disabled() or closed or time < 0.8 or time > 2 or
       pc < 0x3acc8 or pc > 0x3accf then return end
    local sp = cpu.state.SP.value
    if address ~= ((sp + 8) & 0xffff) then return end
    sampling = true
    local page, caller = data:read_u16((sp + 6) & 0xffff), data:read_u16((sp + 7) & 0xffff)
    if page ~= 3 or caller < 0xa93a or caller > 0xa9c6 or (caller - 0xa93a) % 10 ~= 0 then sampling = false; return end
    local ordinal = (counts[caller] or 0) + 1
    counts[caller] = ordinal
    if ordinal <= 8 then
        local low, width = data:read_u16((sp + 9) & 0xffff), data:read_u16((sp + 10) & 0xffff)
        machine:logerror(string.format(
            'mu4_record_header_argument: field=%u ordinal=%u caller=%06x high=%04x low=%04x width=%u receiver=%04x sp=%04x t=%.9f context_pc=%06x boundary=data_read ownership=unvalidated observer_writes=0\n',
            (caller - 0xa93a) // 10, ordinal, 0x30000 + caller, value, low, width, cpu.state.AR1.value,
            sp, time, pc))
    end
    sampling = false
end)
emu.register_frame_done(function()
    if not closed and machine.time:as_double() > 2 then
        for _, tap in ipairs(handles) do tap:remove() end
        closed = true
        machine:logerror(string.format('mu4_record_header_taps_closed: t=%.9f observer_writes=0\n', machine.time:as_double()))
    end
end)
_G.mu4_record_header_taps = handles
