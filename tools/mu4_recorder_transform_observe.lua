-- Read-only AA88 transform boundary; snapshots are context, not retained operands.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'MU4 recorder transform requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = assert(cpu.spaces.data)
local handles, seen = {}, {}
local sampling, closed = false, false

handles[#handles + 1] = data:install_read_tap(0xfc6c, 0xfc6c, 'mu4_record_transform_key', function()
    local pc, time = cpu.state.GENPC.value, machine.time:as_double()
    if sampling or machine:side_effects_disabled() or closed or pc < 0x3f108 or pc > 0x3f10e then return end
    sampling = true
    local sp = cpu.state.SP.value
    local page, caller = data:read_u16((sp + 8) & 0xffff), data:read_u16((sp + 9) & 0xffff)
    -- These are reviewed delayed-call return addresses, not key-expansion calls.
    if page ~= 3 or (caller ~= 0xedae and caller ~= 0xee95) or seen[caller] then sampling = false; return end
    seen[caller] = true
    local target = data:read_u16((sp + 2) & 0xffff)
    local left, right = data:read_u16((sp + 3) & 0xffff), data:read_u16((sp + 4) & 0xffff)
    local state = {}
    for address = 0xfc6c, 0xfcbd do state[#state + 1] = string.format('%04x', data:read_u16(address)) end
    sampling = false
    machine:logerror(string.format(
        'mu4_record_transform_input: caller=%06x target=%04x left=%04x right=%04x state_base=fc6c state=%s sp=%04x t=%.9f context_pc=%06x ownership=unvalidated observer_writes=0\n',
        0x30000 + caller, target, left, right, table.concat(state, ','), sp, time, pc))
    if target < 0x80 or target >= 0xffff then
        machine:logerror('mu4_record_transform_unobserved: target_outside_ram=1 observer_writes=0\n')
        return
    end
    local output, reported = {}, false
    handles[#handles + 1] = data:install_write_tap(target, target + 1, 'mu4_record_transform_output_' .. caller,
        function(address, value, mask)
            if sampling or machine:side_effects_disabled() or reported or closed then return end
            output[address] = value
            machine:logerror(string.format(
                'mu4_record_transform_write: caller=%06x address=%04x value=%04x mask=%04x t=%.9f context_pc=%06x writer_identity=unvalidated observer_writes=0\n',
                0x30000 + caller, address, value, mask, machine.time:as_double(), cpu.state.GENPC.value))
            if output[target] and output[target + 1] then
                reported = true
                machine:logerror(string.format(
                    'mu4_record_transform_output: caller=%06x target=%04x left=%04x right=%04x t=%.9f ownership=unvalidated observer_writes=0 audio_acceptance=0\n',
                    0x30000 + caller, target, output[target], output[target + 1], machine.time:as_double()))
            end
        end)
end)

emu.register_frame_done(function()
    if not closed and machine.time:as_double() > 12 then
        for _, tap in ipairs(handles) do tap:remove() end
        closed = true
        machine:logerror(string.format('mu4_record_transform_taps_closed: count=%u t=%.9f observer_writes=0\n', #handles, machine.time:as_double()))
    end
end)
_G.mu4_record_transform_taps = handles
_G.mu4_record_transform_stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format(
        'mu4_record_transform_terminal: caller_03edae=%u caller_03ee95=%u window_end=12 observer_writes=0 audio_acceptance=0\n',
        seen[0xedae] and 1 or 0, seen[0xee95] and 1 or 0))
end)
