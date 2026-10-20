-- Observe the RAM accounting stores independently of Timer-1 MMIO reads.
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local count=0
_G.nam2_sleep_accounting_tap=cpu.spaces.program:install_write_tap(0x10f9f8,0x10f9fb,'nam2_sleep_accounting',function(_,value,mask)
    local pc=cpu.state['R15'].value
    if mask&0xffff==0 or count>=16 or (pc~=0x2fadac and pc~=0x2fadce) then return end
    count=count+1
    machine:logerror(string.format('nam2_sleep_accounting: index=%d pc=%08x base=%08x target=0010f9fa value=%04x mask=%08x t=%.9f observer_writes=0\n',
        count,pc,cpu.state['R2'].value,value&0xffff,mask,machine.time:as_double()))
end)
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nam2_c3_wake_flow_observe.lua')
