-- Bounded observation of organic serial writes during an established call.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_pin_host_outgoing_call.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local retained={}
local count=0
local function active()
    local time=machine.time:as_double()
    return time>=54 and time<=67 and count<4096
end
retained[#retained+1]=memory:install_read_tap(0x305748,0x30574b,'nsb7_ccont_writer',function()
    if not active() or cpu.state['R15'].value~=0x305748 then return end
    count=count+1
    machine:logerror(string.format('nsb7_ccont_writer: encoded=%08x value=%08x caller=%08x task=%u t=%.9f observer_writes=0\n',
        cpu.state['R0'].value,cpu.state['R1'].value,cpu.state['R14'].value,
        memory:read_u8(0x100022),machine.time:as_double()))
end)
local function observe(kind,address,value,mask)
    if not active() then return end
    for lane=0,3 do
        local offset=(address&255)+lane
        local shift=(3-lane)*8
        if mask&(255<<shift)~=0 and ((kind=='write' and (offset==0x2c or offset==0x2d)) or
                (kind=='read' and offset==0x6c)) then
            count=count+1
            machine:logerror(string.format('nsb7_ccont_serial: kind=%s offset=%02x data=%02x pc=%08x task=%u t=%.9f observer_writes=0\n',
                kind,offset,(value>>shift)&255,cpu.state['R15'].value,
                memory:read_u8(0x100022),machine.time:as_double()))
        end
    end
end
retained[#retained+1]=memory:install_write_tap(0x2002c,0x2002f,'nsb7_ccont_serial_tx',function(a,v,m) observe('write',a,v,m) end)
retained[#retained+1]=memory:install_read_tap(0x2006c,0x2006f,'nsb7_ccont_serial_rx',function(a,v,m) observe('read',a,v,m) end)
_G.nsb7_ccont_writer_taps=retained
