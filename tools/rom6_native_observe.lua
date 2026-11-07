-- Read-only entry observations for the acquired NSM-3 ROM6 composition.
local machine = manager.machine
assert(machine.system.name == 'nsm3r6' or machine.system.name == 'nsm3adc' or machine.system.name == 'nsm3ad0' or machine.system.name == 'nsm3afb')
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
assert(not machine.devices[':dsp_hle'] and not machine.devices[':dsp_staged'])
machine:logerror('rom6_native_composition: hle=0 staged=0 observer_writes=0\n')
local handles = {}
local missing_program=0
handles[#handles+1]=cpu.spaces.program:install_read_tap(0x80,0x1fff,'rom6_program_coverage',function(address)
    if (cpu.state.PMST.value&0x20)==0 then
        missing_program=missing_program+1
        if missing_program<=8 then
            machine:logerror(string.format('rom6_native_unacquired_program: address=%04x observer_writes=0\n',address))
        end
    end
end)
local captured={}
for _,region in ipairs({{'loader2',0x0a00,623,'data'}, {'installed',0x0590,422,'program'}}) do
    local trigger=region[1]=='loader2' and 0x0a00 or 0x2c75
    handles[#handles+1]=cpu.spaces.program:install_read_tap(trigger,trigger,
        'rom6_native_'..region[1],function()
            if captured[region[1]] then return end
            captured[region[1]]=true
            local file=assert(io.open('rom6_native_'..region[1]..'.be16','wb'))
            for address=region[2],region[2]+region[3]-1 do
                file:write(string.pack('>I2',cpu.spaces[region[4]]:read_u16(address)))
            end
            file:close()
            machine:logerror(string.format('rom6_native_upload: region=%s words=%d entry=%04x observer_writes=0\n',
                region[1],region[3],trigger))
        end)
end
local arm=assert(machine.devices[':maincpu'])
machine.debugger:command('temp4=0')
-- Conditions use the breakpoint CPU; logerror expressions use the visible
-- debugger CPU, which a composed DSP observer may change. Qualify ARM memory
-- and retain its pointer in a debugger temporary, never in emulated state.
arm.debug:bpset(0x2cadd0,'maincpu.w@10002!=ffff && temp4<10',
    'temp4=temp4+1;logerror "rom6_native_verifier: result0=%04x result1=%04x observer_writes=0\\n",maincpu.w@10000,maincpu.w@10002;g')
machine.debugger:command('temp3=0')
arm.debug:bpset(0x240e20,'temp3<8 && (temp9=r4)>=0',
    'temp3=temp3+1;logerror "rom6_native_selftest: command=%02x faults=%02x flag=%02x observer_writes=0\\n",maincpu.b@(temp9+8),maincpu.b@(temp9+9),maincpu.b@13fde1;g')
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
    machine:logerror(string.format('rom6_native_coverage: missing_program_reads=%d observer_writes=0\n',missing_program))
end)
_G.rom6_native_observers = handles
machine.debugger:command('g')
