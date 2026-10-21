-- Physical contacts and read-only debugger observations; no guest state writes.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_service_observe.lua')
local machine=manager.machine
local space=machine.devices[':maincpu'].spaces['program']
local mask_writes=0
_G.nsb7_keypad_mask_tap=space:install_write_tap(0x20068,0x2006b,'nsb7_keypad_mask',function(_,value,mask)
    if mask&0xff==0 or mask_writes>=64 then return end
    mask_writes=mask_writes+1
    local cpu=machine.devices[':maincpu']
    machine:logerror(string.format('nsb7_key_mask: pc=%08x value=%02x stack_return=%08x t=%.9f observer_writes=0\n',
        cpu.state['R15'].value,value&0xff,space:read_u32(cpu.state['R13'].value),machine.time:as_double()))
end)
local startup_taps={}
for _,address in ipairs({0x1308b1,0x13ff00}) do
    local count=0
    local shift=(3-(address&3))*8
    startup_taps[#startup_taps+1]=space:install_write_tap(address&~3,(address&~3)+3,'nsb7_startup_'..address,function(_,value,mask)
        if mask&(0xff<<shift)==0 or count>=64 then return end
        count=count+1
        machine:logerror(string.format('nsb7_startup_write: address=%08x value=%02x pc=%08x t=%.9f observer_writes=0\n',
            address,(value>>shift)&0xff,machine.devices[':maincpu'].state['R15'].value,machine.time:as_double()))
    end)
end
_G.nsb7_input_startup_taps=startup_taps
local function registers(phase)
    machine:logerror(string.format('nsb7_input_regs: phase=%s row=%02x direction=%02x column_mask=%02x irq=%02x irq_mask=%02x ctrl=%02x observer_writes=0\n',
        phase,space:read_u8(0x20028),space:read_u8(0x200a8),space:read_u8(0x2006b),
        space:read_u8(0x20009),space:read_u8(0x2000b),space:read_u8(0x2000c)))
    machine:logerror(string.format('nsb7_startup_state: selector=%02x state=%02x reports=%02x observer_writes=0\n',
        space:read_u8(0x13fec1),space:read_u8(0x13ff00),space:read_u8(0x1308b1)))
end
machine.debugger.visible_cpu=machine.devices[':maincpu']
machine.debugger:command('temp8=0')
machine.debugger:command('temp9=0')
machine.debugger:command('bpset 30835c,1,{logerror "nsb7_key_init: observer_writes=0\\n";g}')
machine.debugger:command('bpset 30833a,1,{logerror "nsb7_key_irq: observer_writes=0\\n";g}')
machine.debugger:command('bpset 30ad50,r0!=0xff && temp8<32,{temp8=temp8+1;logerror "nsb7_key_raw: raw=%02x observer_writes=0\\n",r0;g}')
machine.debugger:command('bpset 30ad80,temp9<32,{temp9=temp9+1;logerror "nsb7_key_decode: key=%02x observer_writes=0\\n",r0;g}')
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local input=coroutine.create(function()
    for _,step in ipairs({{8,'Left Softkey / Menu'},{10,'Keypad 1'}}) do
        assert(emu.wait(step[1]-machine.time:as_double()))
        local key=assert(fields[step[2]],'physical key missing')
        registers('before')
        machine:logerror(string.format('nsb7_input: key=%s state=1 t=%.9f\n',step[2],machine.time:as_double()))
        key:set_value(1)
        assert(emu.wait(0.2))
        registers('held')
        key:set_value(0)
        machine:logerror(string.format('nsb7_input: key=%s state=0 t=%.9f\n',step[2],machine.time:as_double()))
        assert(emu.wait(0.3))
        machine.screens[':screen']:snapshot('nsb7_key_'..step[1]..'.png')
    end
    assert(emu.wait(1))
    machine:exit()
end)
_G.nsb7_physical_input=input
assert(coroutine.resume(input))
