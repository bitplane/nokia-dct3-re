-- Passive RAM snapshots at the actual software stop-request publication.
-- PC values are context, not retained instruction ownership.
local machine = manager.machine
assert(machine.system.name == 'mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = cpu.spaces.data
local captures = 0
local function words(base, count)
    local result = {}
    for offset=0,count-1 do result[#result+1] = string.format('%04x', data:read_u16(base+offset)) end
    return table.concat(result, ',')
end
local request = data:install_write_tap(0x05eb,0x05eb,'mu4_recorder_tail_request',
    function(_,value,mask)
        if machine:side_effects_disabled() or mask ~= 0xffff or value ~= 1 or machine.time:as_double() < 1 then return end
        captures = captures+1
        if captures > 4 then return end
        local object = data:read_u16(0x05f4)
        assert(object >= 0x80 and object <= 0xfffe, 'tail object outside RAM')
        local table_base = data:read_u16(object)
        assert(table_base >= 0x80 and table_base <= 0xffe2, 'tail method table outside RAM')
        local ed, ee, held = data:read_u16(0x05ed),data:read_u16(0x05ee),data:read_u16(0xfd70)
        machine:logerror(string.format('mu4_recorder_tail_request: ordinal=%u ed=%04x ee=%04x held=%04x object=%04x table=%04x finish=%s process=%s buffer=%s carry=%s t=%.9f context_pc=%06x observer_writes=0\n',
            captures,ed,ee,held,object,table_base,words(table_base+24,2),words(table_base+28,2),
            words(0x061c,math.min(ed,760)),words(0xfd4e,math.min(held,16)),machine.time:as_double(),cpu.state.GENPC.value))
    end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('mu4_recorder_tail_terminal: captures=%u capped=%u ed=%04x ee=%04x held=%04x illegal=%u t=%.9f observer_writes=0\n',
        captures,captures>4 and 1 or 0,data:read_u16(0x05ed),data:read_u16(0x05ee),data:read_u16(0xfd70),cpu.state.ILLEGAL.value,machine.time:as_double()))
end)
_G.mu4_recorder_tail_handles = {request,stop}
