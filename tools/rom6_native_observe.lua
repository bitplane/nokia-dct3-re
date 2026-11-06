-- Read-only entry observations for the acquired NSM-3 ROM6 composition.
local machine = manager.machine
assert(machine.system.name == 'nsm3r6')
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
assert(not machine.devices[':dsp_hle'] and not machine.devices[':dsp_staged'])
local handles = {}
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
    if second ~= last and second <= 30 then last=second; snapshot('periodic') end
end)
handles[#handles+1] = emu.add_machine_stop_notifier(function()
    snapshot('terminal')
    -- This uploaded instruction is a CPU-admission boundary, not ROM data.
    local words = {}
    for address=0x0b38,0x0b3f do
        words[#words+1] = string.format('%04x',cpu.spaces.program:read_u16(address))
    end
    machine:logerror('rom6_native_program: address=0b38 words='..table.concat(words,',')..' observer_writes=0\n')
end)
_G.rom6_native_observers = handles
machine.debugger:command('g')
