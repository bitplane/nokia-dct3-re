-- Passive branch-entry observations for the own NSM-5 measurement contract.
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
local taps = {}
for _, address in ipairs({0x37019c, 0x2e336c, 0x2bd782,
                         0x2e1f5e, 0x2bd3d8, 0x2bd424}) do
    local count = 0
    taps[#taps + 1] = cpu.spaces['program']:install_read_tap(address & ~3,
      (address & ~3) + 3, '5210_measurement_entry', function(offset, data, mask)
        if cpu.state['R15'].value ~= address then return end
        count = count + 1
        if count <= 8 then
            machine:logerror(string.format(
                '%s: pc=%08x r0=%08x r1=%08x t=%.9f\n',
                (address == 0x37019c or address == 0x2e336c or address == 0x2bd782)
                    and '5210_measurement_entry' or '5210_request_entry',
                address, cpu.state['R0'].value, cpu.state['R1'].value,
                machine.time:as_double()))
        end
      end)
end
_G.noki5210_measurement_observers = taps
local selections = 0
taps[#taps + 1] = cpu.spaces['program']:install_read_tap(0x2e23ac,
  0x2e23af, '5210_readiness_selector', function(offset, data, mask)
    if cpu.state['R15'].value ~= 0x2e23ac then return end
    selections = selections + 1
    if selections <= 64 then
        local memory = cpu.spaces['program']
        machine:logerror(string.format(
            '5210_readiness_selector: argument=%08x lr=%08x state=%d eligible=%d current=%08x queued=%08x t=%.9f\n',
            cpu.state['R0'].value, cpu.state['R14'].value,
            memory:read_u16(0x13e78e), memory:read_u8(0x13c52e),
            memory:read_u32(0x13e794), memory:read_u32(0x13e798),
            machine.time:as_double()))
        local context = cpu.state['R4'].value
        if context >= 0x100000 and context <= 0x17fff0 then
            local bytes = {}
            for index = 0, 15 do
                bytes[#bytes + 1] = string.format('%02x', memory:read_u8(context + index))
            end
            machine:logerror(string.format(
                '5210_readiness_receive_context: lr=%08x address=%08x data=%s\n',
                cpu.state['R14'].value, context, table.concat(bytes)))
        end
    end
  end)
local writes = 0
taps[#taps + 1] = cpu.spaces['program']:install_write_tap(0x13e78c,
  0x13e7a3, '5210_readiness_root', function(offset, data, mask)
    writes = writes + 1
    if writes <= 80 then
        machine:logerror(string.format(
            '5210_readiness_root: address=%08x data=%08x mask=%08x pc=%08x lr=%08x t=%.9f\n',
            offset, data, mask, cpu.state['R15'].value, cpu.state['R14'].value,
            machine.time:as_double()))
    end
  end)
taps[#taps + 1] = emu.add_machine_stop_notifier(function()
    local memory = cpu.spaces['program']
    machine:logerror(string.format(
        '5210_readiness_root: final current=%08x queued=%08x previous=%08x context=%08x writes=%d selections=%d\n',
        memory:read_u32(0x13e794), memory:read_u32(0x13e798),
        memory:read_u32(0x13e79c), memory:read_u32(0x13e7a0), writes, selections))
    for _, role in ipairs({{'current', 0x13e794}, {'queued', 0x13e798}}) do
        local pointer = memory:read_u32(role[2])
        if pointer >= 0x100000 and pointer <= 0x17fff0 then
            local bytes = {}
            for offset = 0, 15 do
                bytes[#bytes + 1] = string.format('%02x', memory:read_u8(pointer + offset))
            end
            machine:logerror('5210_readiness_object: role=' .. role[1] ..
                ' data=' .. table.concat(bytes) .. '\n')
        end
    end
end)
