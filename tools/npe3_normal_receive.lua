-- Bounded read-only class-50 allocation and correlator observations.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local handles={}
local classes,windows=0,0
local function word(at) return cpu.spaces.data:read_u16(at) end
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x2f02,0x2f02,'npe3_receive_class',function(_,value)
    local now=machine.time:as_double()
    if now<3.2 or now>4 or value~=0x50 then return end
    classes=classes+1
    if classes>64 then return end
    machine:logerror(string.format('npe3_receive_class: sequence=%d t=%.9f value=%04x execute=%06x read=%06x mode=%04x frame=%04x tsc=%04x,%04x completed=%04x observer_writes=0\n',classes,now,value,cpu.state.EXECPC.value,cpu.state.READPC.value,word(0x250f),word(0x913),word(0x250b),word(0x2616),word(0x231)))
end)
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x10,0x10,'npe3_normal_training',function(_,value)
    local owner=cpu.state.EXECPC.value
    if windows>=4 or machine.time:as_double()>4 or value~=0x7737 or owner<0x76a7 or owner>0x76d9 or word(0x2f02)~=0x50 then return end
    windows=windows+1
    local file=assert(io.open(string.format('npe3_normal_training_%d.be16',windows),'wb'))
    for at=0,0xffff do file:write(string.pack('>I2',word(at))) end
    file:close()
    machine:logerror(string.format('npe3_normal_training: sequence=%d t=%.9f owner=%06x source=%04x mode=%04x frame=%04x tsc=%04x,%04x completed=%04x observer_writes=0\n',windows,machine.time:as_double(),owner,cpu.state.AR3.value,word(0x250f),word(0x913),word(0x250b),word(0x2616),word(0x231)))
end)
_G.npe3_normal_receive=handles
