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
