-- Passive AA88 caller frames selected by argument reads; no guest writes.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'recorder header observation requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = assert(cpu.spaces.data)
local session = _G.mu4_record_header_full_session == true
local last, cap = session and 15 or 2, session and 512 or 8
local marker = session and 'mu4_record_session_argument' or 'mu4_record_header_argument'
local sampling, closed = false, false
local handles, counts = {}, {}
local total, capped = 0, false
if session then
    machine:logerror('mu4_record_session_scope: first=0.8 last=15 fields=15 max_frames=512 boundary=data_read context_pc=not_owner observer_writes=0\n')
end
handles[1] = data:install_read_tap(0x6000, 0x7fff, 'mu4_recorder_header_arguments', function(address, value)
    local pc, time = cpu.state.GENPC.value, machine.time:as_double()
    if sampling or machine:side_effects_disabled() or closed or time < 0.8 or time > last or
       pc < 0x3acc8 or pc > 0x3accf then return end
    local sp = cpu.state.SP.value
    if address ~= ((sp + 8) & 0xffff) then return end
    sampling = true
    local page, caller = data:read_u16((sp + 6) & 0xffff), data:read_u16((sp + 7) & 0xffff)
    if page ~= 3 or caller < 0xa93a or caller > 0xa9c6 or (caller - 0xa93a) % 10 ~= 0 then sampling = false; return end
    local ordinal = (counts[caller] or 0) + 1
    counts[caller] = ordinal
    total = total+1
    if ordinal > cap then capped = true end
    if ordinal <= cap then
        local low, width = data:read_u16((sp + 9) & 0xffff), data:read_u16((sp + 10) & 0xffff)
        machine:logerror(string.format(
            '%s: field=%u ordinal=%u caller=%06x high=%04x low=%04x width=%u receiver=%04x sp=%04x t=%.9f context_pc=%06x boundary=data_read ownership=unvalidated observer_writes=0\n',
            marker,(caller - 0xa93a) // 10, ordinal, 0x30000 + caller, value, low, width, cpu.state.AR1.value,
            sp, time, pc))
    end
    sampling = false
end)
emu.register_frame_done(function()
    if not closed and machine.time:as_double() > last then
        for _, tap in ipairs(handles) do tap:remove() end
        closed = true
        if session then
            local fields = {}
            for field = 0,14 do fields[#fields+1] = tostring(counts[0xa93a+10*field] or 0) end
            machine:logerror(string.format('mu4_record_session_closed: arguments=%u counts=%s capped=%u t=%.9f observer_writes=0\n',
                total,table.concat(fields,','),capped and 1 or 0,machine.time:as_double()))
        else
            machine:logerror(string.format('mu4_record_header_taps_closed: t=%.9f observer_writes=0\n', machine.time:as_double()))
        end
    end
end)
_G.mu4_record_header_taps = handles
if session then
    _G.mu4_record_header_stop = emu.add_machine_stop_notifier(function()
        machine:logerror(string.format('mu4_record_session_terminal: closed=%u arguments=%u capped=%u illegal=%u observer_writes=0\n',
            closed and 1 or 0,total,capped and 1 or 0,cpu.state.ILLEGAL.value))
    end)
end
