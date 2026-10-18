-- Observe existing bus transactions; extra reads touch only ordinary RAM.
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
machine:logerror('rom4_staged_profile: system=' .. machine.system.name .. '\n')
local captured = false
local stop_notifier
local low_reads, low_writes = 0, 0
local gap_reads = 0
local host_consumed = 0
local host_headers = 0
local header_tap = cpu.spaces.data:install_read_tap(0x0800, 0x0800, 'staged_host_header', function(address, value, mask)
    host_headers = host_headers + 1
    if host_headers <= 16 then
        machine:logerror(string.format('rom4_staged_host_header: index=%d value=%04x mask=%04x pc_context=%04x t=%.9f\n',
            host_headers, value, mask, cpu.state.PC.value, machine.time:as_double()))
    end
end)
local host_tap = cpu.spaces.data:install_write_tap(0x0853, 0x0853, 'staged_host_consumer', function(address, value, mask)
    local previous = cpu.spaces.data:read_u16(address)
    local combined = (previous & ~mask) | (value & mask)
    if combined == previous then return end
    host_consumed = host_consumed + 1
    if host_consumed <= 8 then
        machine:logerror(string.format('rom4_staged_host_consume: index=%d previous=%04x next=%04x pc_context=%04x t=%.9f\n',
            host_consumed, previous, combined, cpu.state.PC.value, machine.time:as_double()))
    end
end)
local gap_read_tap = cpu.spaces.data:install_read_tap(0x0003, 0x0003, 'staged_unowned_low_read', function(address, value, mask)
    gap_reads = gap_reads + 1
    if gap_reads <= 64 then
        machine:logerror(string.format('rom4_staged_gap_read: address=%04x value=%04x mask=%04x pc_context=%04x t=%.9f\n',
            address, value, mask, cpu.state.PC.value, machine.time:as_double()))
    end
end)
local low_read_tap = cpu.spaces.data:install_read_tap(0x00f1, 0x00f2, 'staged_low_pointer_read', function(address, value, mask)
    low_reads = low_reads + 1
    if low_reads <= 64 then
        machine:logerror(string.format('rom4_staged_low_read: address=%04x value=%04x mask=%04x pc_context=%04x t=%.9f\n',
            address, value, mask, cpu.state.PC.value, machine.time:as_double()))
    end
end)
local low_write_tap = cpu.spaces.data:install_write_tap(0x00f1, 0x00f2, 'staged_low_pointer_write', function(address, value, mask)
    low_writes = low_writes + 1
    if low_writes <= 64 then
        machine:logerror(string.format('rom4_staged_low_write: address=%04x value=%04x mask=%04x pc_context=%04x t=%.9f\n',
            address, value, mask, cpu.state.PC.value, machine.time:as_double()))
    end
end)
local bsp_tap = cpu.spaces.data:install_write_tap(0x22, 0x22, 'staged_bsp_control', function(_, value, mask)
    machine:logerror(string.format('rom4_staged_bsp_write: value=%04x mask=%04x t=%.9f\n',
        value, mask, machine.time:as_double()))
end)
local tx_count = 0
local tx_tap = cpu.spaces.data:install_write_tap(0x21, 0x21, 'staged_bsp_transmit', function(_, value, mask)
    tx_count = tx_count + 1
    if tx_count <= 16 then
        machine:logerror(string.format('rom4_staged_bsp_tx: index=%d value=%04x mask=%04x t=%.9f\n',
            tx_count, value, mask, machine.time:as_double()))
    end
end)
local rx_count = 0
local rx_tap = cpu.spaces.data:install_read_tap(0x20, 0x20, 'staged_bsp_receive', function(_, value, mask)
    rx_count = rx_count + 1
    if rx_count <= 16 then
        machine:logerror(string.format('rom4_staged_bsp_rx: index=%d value=%04x mask=%04x t=%.9f\n',
            rx_count, value, mask, machine.time:as_double()))
    end
end)
emu.register_frame_done(function()
    assert(bsp_tap and tx_tap and rx_tap and low_read_tap and low_write_tap and gap_read_tap and host_tap and header_tap and stop_notifier)
    if captured or machine.time:as_double() < 0.5 then return end
    captured = true
    machine:logerror(string.format('rom4_staged_low_count: reads=%d writes=%d\n', low_reads, low_writes))
    machine:logerror(string.format('rom4_staged_gap_count: reads=%d\n', gap_reads))
    machine:logerror(string.format('rom4_staged_bsp_tx_count: count=%d\n', tx_count))
    machine:logerror(string.format('rom4_staged_bsp_rx_count: count=%d\n', rx_count))
    local words = {}
    for address = 0x0f00, 0x0f27 do
        words[#words + 1] = string.format('%04x', cpu.spaces.program:read_u16(address))
    end
    machine:logerror(string.format(
        'rom4_staged_frontier: pc=%04x xpc=%02x illegal=%d pmst=%04x swwsr=%04x bscr=%04x swcr=%04x words=%s t=%.9f\n',
        cpu.state.PC.value, cpu.state.XPC.value, cpu.state.ILLEGAL.value,
        cpu.state.PMST.value, cpu.state.SWWSR.value, cpu.state.BSCR.value,
        cpu.state.SWCR.value, table.concat(words, ','), machine.time:as_double()))
    machine:logerror(string.format(
        'rom4_staged_dagen: bk=%04x ar0=%04x ar2=%04x st0=%04x st1=%04x\n',
        cpu.state.BK.value, cpu.state.AR0.value, cpu.state.AR2.value,
        cpu.state.ST0.value, cpu.state.ST1.value))
    -- This checkpoint can precede a later rejection. It does not identify
    -- retirement operands; a rejected Decode may leave older slots pending.
    machine:logerror(string.format(
        'rom4_staged_accumulators: a=%010x b=%010x t=%04x ar3=%04x ar4=%04x\n',
        cpu.state.A.value, cpu.state.B.value, cpu.state.T.value,
        cpu.state.AR3.value, cpu.state.AR4.value))
    local tail = {}
    for address = 0x0e10, 0x0e27 do
        tail[#tail + 1] = string.format('%04x', cpu.spaces.program:read_u16(address))
    end
    machine:logerror('rom4_staged_uploaded_tail: base=0e10 words=' .. table.concat(tail, ',') .. '\n')
    local loop = {}
    for address = 0x0e58, 0x0e6f do
        loop[#loop + 1] = string.format('%04x', cpu.spaces.program:read_u16(address))
    end
    machine:logerror('rom4_staged_repeat_tail: base=0e58 words=' .. table.concat(loop, ',') .. '\n')
end)
stop_notifier = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format(
        'rom4_staged_end: pc=%04x xpc=%02x illegal=%d reads=%d writes=%d gap_reads=%d host_consumed=%d host_headers=%d t=%.9f\n',
        cpu.state.PC.value, cpu.state.XPC.value, cpu.state.ILLEGAL.value,
        low_reads, low_writes, gap_reads, host_consumed, host_headers, machine.time:as_double()))
    machine:logerror(string.format(
        'rom4_staged_end_state: a=%010x b=%010x t=%04x st0=%04x st1=%04x bk=%04x ar0=%04x ar1=%04x ar2=%04x ar3=%04x\n',
        cpu.state.A.value, cpu.state.B.value, cpu.state.T.value,
        cpu.state.ST0.value, cpu.state.ST1.value, cpu.state.BK.value,
        cpu.state.AR0.value, cpu.state.AR1.value, cpu.state.AR2.value, cpu.state.AR3.value))
    local demand = {}
    for address = 0x0a40, 0x0a5f do
        demand[#demand + 1] = string.format('%04x', cpu.spaces.program:read_u16(address))
    end
    machine:logerror('rom4_staged_demand_tail: base=0a40 words=' .. table.concat(demand, ',') .. '\n')
end)
