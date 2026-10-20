-- Observe both early clock-stop and later declined-sleep writes; no MMIO writes.
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local commands={}
_G.nam2_clock_zero_tap=memory:install_write_tap(0x2000c,0x2000f,'nam2_clock_zero',function(offset,value,mask)
    if mask & 0x00ff0000 == 0 then return end
    local data=(value >> 16) & 0xff
    local caller=cpu.state['PC'].value==0x304c56 and memory:read_u32(cpu.state['R13'].value+4) or 0
    local key=string.format('%08x:%02x',cpu.state['PC'].value,data)
    commands[key]=(commands[key] or 0)+1
    if commands[key]<=2 then
        machine:logerror(string.format('nam2_clock_zero_write: pc=%08x data=%02x ctrl=%02x caller=%08x t=%.9f observer_writes=0\n',cpu.state['PC'].value,data,memory:read_u8(0x2000c),caller,machine.time:as_double()))
        local head=memory:read_u32(0x100034)
        local remaining=head>=0x100000 and head<0x180000 and memory:read_u16(head+4) or 0xffff
        machine:logerror(string.format('nam2_clock_guard_state: data=%02x flags=%02x head=%08x remaining=%04x timer0=%04x stamp=%04x timer1=%04x destination=%04x fiq=%02x fmask=%02x irq=%02x imask=%02x t=%.9f observer_writes=0\n',
            data,memory:read_u8(0x100022),head,remaining,memory:read_u16(0x20010),memory:read_u16(0x10f9f8),memory:read_u16(0x20004),memory:read_u16(0x20006),
            memory:read_u8(0x20008),memory:read_u8(0x2000a),memory:read_u8(0x20009),memory:read_u8(0x2000b),machine.time:as_double()))
    end
end)
local reported=false
emu.register_frame_done(function()
    if not reported and machine.time:as_double()>=29 then
        reported=true
        for key,count in pairs(commands) do machine:logerror(string.format('nam2_clock_zero_total: site=%s count=%d observer_writes=0\n',key,count)) end
    end
end)
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nam2_c3_no_sim_idle_observe.lua')
