-- Enter the own-ROM fallback code through physical matrix contacts only.
_G.nsb7_observe_only=true
local source=debug.getinfo(1,'S').source:sub(2)
if _G.nsb7_security_with_pin then
    _G.nsb7_correct_pin=true
    _G.nsb7_pin_continue=true
    dofile(source:match('^(.*[/\\])')..'nsb7_pin_input.lua')
else
    dofile(source:match('^(.*[/\\])')..'nsb7_sim_observe.lua')
end
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local taps={}
for _,pc in ipairs({0x295a40,0x295a88}) do
    local count=0
    taps[#taps+1]=memory:install_read_tap(pc,pc+3,'nsb7_security_'..pc,function()
        if cpu.state['R15'].value~=pc or count>=16 then return end
        count=count+1
        local bytes={}
        local pointer=cpu.state[pc==0x295a40 and 'R6' or 'R5'].value
        for i=0,(pc==0x295a40 and 2 or 4) do
            bytes[#bytes+1]=string.format('%02x',memory:read_u8(pointer+i))
        end
        machine:logerror(string.format('nsb7_security_read: pc=%08x data=%s valid=%u t=%.9f observer_writes=0\n',
            pc,table.concat(bytes),cpu.state['R7'].value,machine.time:as_double()))
    end)
end
_G.nsb7_security_taps=taps
local nv_reads=0
taps[#taps+1]=memory:install_read_tap(0x30d658,0x30d65b,'nsb7_security_nv_source',function()
    local destination=cpu.state['R1'].value
    if cpu.state['R15'].value~=0x30d658 or destination<0x130a8c or destination>=0x130a94 or nv_reads>=16 then return end
    nv_reads=nv_reads+1
    machine:logerror(string.format('nsb7_security_nv_read: offset=%04x destination=%08x length=%u t=%.9f observer_writes=0\n',
        cpu.state['R0'].value,destination,cpu.state['R2'].value,machine.time:as_double()))
end)
local verdicts=0
taps[#taps+1]=memory:install_read_tap(0x302e24,0x302e27,'nsb7_security_retry_gate',function()
    if cpu.state['R15'].value~=0x302e24 or verdicts>=16 then return end
    verdicts=verdicts+1
    machine:logerror(string.format('nsb7_security_retry: match=%u retries=%02x t=%.9f observer_writes=0\n',
        cpu.state['R0'].value,memory:read_u8(0x130a90),machine.time:as_double()))
end)
local comparisons=0
taps[#taps+1]=memory:install_read_tap(0x302ea0,0x302ea3,'nsb7_security_compare',function()
    if cpu.state['R15'].value~=0x302ea2 or comparisons>=16 then return end
    comparisons=comparisons+1
    local chars={}
    for offset=0,5 do chars[#chars+1]=string.format('%02x',memory:read_u8(cpu.state['R4'].value+offset)) end
    machine:logerror(string.format('nsb7_security_compare: input=%s encoded=%08x stored=%08x t=%.9f observer_writes=0\n',
        table.concat(chars),memory:read_u32(cpu.state['R13'].value),memory:read_u32(0x130a8c),machine.time:as_double()))
end)
local writes=0
taps[#taps+1]=memory:install_write_tap(0x130a8c,0x130a93,'nsb7_security_record',function(address,value,mask)
    if writes>=32 then return end
    writes=writes+1
    machine:logerror(string.format('nsb7_security_write: address=%08x data=%08x mask=%08x pc=%08x t=%.9f observer_writes=0\n',
        address,value,mask,cpu.state['R15'].value,machine.time:as_double()))
end)
local input=coroutine.create(function()
    local delay=_G.nsb7_security_with_pin and 8 or 0
    for index,key in ipairs({'Keypad 1','Keypad 2','Keypad 3','Keypad 4','Keypad 5','Left Softkey / Menu'}) do
        assert(emu.wait(7+index+delay-machine.time:as_double()))
        local field=assert(fields[key])
        machine:logerror(string.format('nsb7_security_input: key=%s state=1 t=%.9f\n',key,machine.time:as_double()))
        field:set_value(1)
        assert(emu.wait(0.2))
        field:set_value(0)
        machine:logerror(string.format('nsb7_security_input: key=%s state=0 t=%.9f\n',key,machine.time:as_double()))
        assert(emu.wait(0.3))
        machine.screens[':screen']:snapshot('nsb7_security_key_'..index..'.png')
    end
    for _,base in ipairs({14,18,25}) do
        local target=base+delay
        -- Loading cancels passive capture waits; it must not report a failed key action.
        if not emu.wait(target-machine.time:as_double()) then return end
        machine.screens[':screen']:snapshot('nsb7_security_'..target..'.png')
    end
    if not _G.nsb7_security_continue then machine:exit() end
end)
_G.nsb7_security_input=input
assert(coroutine.resume(input))
