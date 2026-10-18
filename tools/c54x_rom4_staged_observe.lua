-- Observe existing BSP transactions; extra reads touch only ordinary uploaded RAM.
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local captured = false
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
    assert(bsp_tap and tx_tap and rx_tap)
    if captured or machine.time:as_double() < 0.5 then return end
    captured = true
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
