-- Bounded read-only class-50 allocation and FIRE result observations.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local handles={}
local classes,checks=0,0
local last_time=(machine.system.name=='npe3sp9' or machine.system.name=='npe3tp5' or machine.system.name=='npe3ap5') and 8 or 4
local function word(at) return cpu.spaces.data:read_u16(at) end
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x2f02,0x2f02,'npe3_receive_class',function(_,value)
    local now=machine.time:as_double()
    if now<3.2 or now>last_time or value~=0x50 then return end
    classes=classes+1
    if classes>64 then return end
    machine:logerror(string.format('npe3_receive_class: sequence=%d t=%.9f value=%04x execute=%06x read=%06x mode=%04x frame=%04x tsc=%04x,%04x completed=%04x observer_writes=0\n',classes,now,value,cpu.state.EXECPC.value,cpu.state.READPC.value,word(0x250f),word(0x913),word(0x250b),word(0x2616),word(0x231)))
end)
handles[#handles+1]=cpu.spaces.io:install_read_tap(0x38,0x38,'npe3_bcch_fire',function(_,value)
    if checks>=64 or machine.time:as_double()<3.2 or machine.time:as_double()>last_time then return end
    checks=checks+1
    local decoded={}
    for at=0x2f80,0x2f8d do decoded[#decoded+1]=string.format('%04x',word(at)) end
    machine:logerror(string.format('npe3_bcch_fire: sequence=%d t=%.9f status=%04x frame=%04x words=%s read_owner=%06x observer_writes=0\n',checks,machine.time:as_double(),value,word(0x913),table.concat(decoded,','),cpu.state.READPC.value))
end)
_G.npe3_normal_receive=handles
