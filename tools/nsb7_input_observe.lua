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
local analog_count=0
_G.nsb7_input_analog_tap=space:install_write_tap(0x130020,0x130023,'nsb7_analog_state',function(_,value,mask)
    if mask&0xffff0000==0 or analog_count>=64 then return end
    analog_count=analog_count+1
    machine:logerror(string.format('nsb7_analog_state: value=%04x pc=%08x t=%.9f observer_writes=0\n',
        (value>>16)&0xffff,machine.devices[':maincpu'].state['R15'].value,machine.time:as_double()))
end)
local text_count=0
local seen_text={}
_G.nsb7_input_analog_text=space:install_read_tap(0x30ae80,0x30ae83,'nsb7_analog_text',function()
    local cpu=machine.devices[':maincpu']
    local address=cpu.state['R0'].value
    if cpu.state['R15'].value~=0x30ae82 or address<0x245000 or address>=0x248600 or text_count>=100 then return end
    local chars={}
    for offset=0,95 do
        local byte=space:read_u8(address+offset)
        if byte==0 then break end
        chars[#chars+1]=byte>=32 and byte<=126 and string.char(byte) or '?'
    end
    local text=table.concat(chars)
    if text=='' or seen_text[text] then return end
    seen_text[text]=true
    text_count=text_count+1
    machine:logerror(string.format('nsb7_analog_text: text=%s t=%.9f observer_writes=0\n',text,machine.time:as_double()))
end)
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
machine.debugger:command('bpset 30ad50,r0!=0xff && temp8<128,{temp8=temp8+1;logerror "nsb7_key_raw: raw=%02x observer_writes=0\\n",r0;g}')
machine.debugger:command('bpset 30ad80,temp9<128,{temp9=temp9+1;logerror "nsb7_key_decode: key=%02x observer_writes=0\\n",r0;g}')
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
-- The baseline input fixture has an empty physical socket. The separate SIM
-- observer retains the configured card; neither fixture writes guest state.
if not _G.nsb7_observe_inserted_card then
    assert(fields['Remove SIM card']):set_value(1)
end
machine:logerror('nsb7_card_fixture: present='..(_G.nsb7_observe_inserted_card and '1' or '0')..' physical_contact=1\n')
local input=coroutine.create(function()
    if _G.nsb7_observe_only then return end
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
