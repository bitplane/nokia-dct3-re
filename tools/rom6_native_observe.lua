-- Read-only entry observations for the acquired NSM-3 ROM6 composition.
local machine = manager.machine
assert(machine.system.name == 'nsm3r6')
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
assert(not machine.devices[':dsp_hle'] and not machine.devices[':dsp_staged'])
local handles = {}
local arm=assert(machine.devices[':maincpu'])
machine.debugger:command('temp4=0')
arm.debug:bpset(0x2cadd0,'w@10002!=ffff && temp4<10',
    'temp4=temp4+1;logerror "rom6_native_verifier: result0=%04x result1=%04x observer_writes=0\\n",w@10000,w@10002;g')
machine.debugger:command('temp3=0')
arm.debug:bpset(0x240e20,'temp3<8',
    'temp3=temp3+1;logerror "rom6_native_selftest: command=%02x faults=%02x flag=%02x observer_writes=0\\n",b@(r4+8),b@(r4+9),b@13fde1;g')
for index, site in ipairs({{0xff80,'reset'}, {0x0f00,'upload'}, {0x2c75,'resident'}}) do
    local counter = 'temp'..(index-1)
    machine.debugger:command(counter..'=0')
    -- Debugger integers are hexadecimal: 10 caps each site at sixteen entries.
    handles[#handles+1] = cpu.debug:bpset(site[1], counter..'<10',
        counter..'='..counter..'+1;logerror "rom6_native_entry: kind='..site[2]..
        ' address='..string.format('%04x',site[1])..
        ' ordinal=%d identity=Execute_entry observer_writes=0\\n",'..counter..';g')
end
local function snapshot(kind)
    machine:logerror(string.format('rom6_native_state: kind=%s t=%.6f pc=%06x illegal=%d idle=%d pmst=%04x st1=%04x imr=%04x ifr=%04x hle=0 observer_writes=0\n',
        kind,machine.time:as_double(),cpu.state['GENPC'].value,cpu.state['ILLEGAL'].value,
        cpu.state['IDLE'].value,cpu.state['PMST'].value,cpu.state['ST1'].value,
        cpu.state['IMR'].value,cpu.state['IFR'].value))
end
local last = -1
handles[#handles+1] = emu.add_machine_frame_notifier(function()
    local second = math.floor(machine.time:as_double())
    if second ~= last and second <= 30 then
        last=second; snapshot('periodic')
        if second==4 or second==8 or second==16 or second==30 then
            machine.screens[':screen']:snapshot(string.format('rom6_native_%02d.png',second))
        end
    end
end)
handles[#handles+1] = emu.add_machine_stop_notifier(function()
    snapshot('terminal')
    machine.debugger:command('dasm dsp-init.asm,a80,100,1,:dsp_c54x:cpu')
    -- This uploaded instruction is a CPU-admission boundary, not ROM data.
    local words = {}
    for address=0x0b30,0x0b5f do
        words[#words+1] = string.format('%04x',cpu.spaces.program:read_u16(address))
    end
    machine:logerror('rom6_native_program: address=0b30 words='..table.concat(words,',')..' observer_writes=0\n')
    machine:logerror(string.format('rom6_native_context: ar3=%04x ar4=%04x brc=%04x observer_writes=0\n',
        cpu.state.AR3.value,cpu.state.AR4.value,cpu.state.BRC.value))
    local data={}
    for address=0x60,0x6f do data[#data+1]=string.format('%04x',cpu.spaces.data:read_u16(address)) end
    machine:logerror('rom6_native_data: address=0060 words='..table.concat(data,',')..' observer_writes=0\n')
end)
_G.rom6_native_observers = handles
machine.debugger:command('g')
