-- Observe own factory-record consumers; never manufacture a DSP reply.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
dofile(directory .. 'noki5510_identity_observe.lua')
local machine, cpu = manager.machine, manager.machine.devices[':maincpu']
machine.debugger:command('temp3=0;temp4=0;temp5=0')
machine:logerror('5510_records_scope: reply34=0029fdc4 reply35=0029ffe8 state=001204dc cap=16 observer_writes=0\n')
local handles = {}
handles[#handles+1] = cpu.debug:bpset(0x29f876,'temp3<10',
    'temp3=temp3+1;logerror "5510_phone_code_entry: ordinal=%d destination=%08x sp=%08x observer_writes=0\\n",temp3,r1,r13;g')
handles[#handles+1] = cpu.debug:bpset(0x29f8e2,'temp3>0 && temp3<11',
    'logerror "5510_phone_code_return: ordinal=%d destination=%08x sp=%08x stored_valid=%d output=%08x%02x terminator=%02x observer_writes=0\\n",temp3,r5,r13,r0,d@r5,b@(r5+4),b@(r5+5);g')
handles[#handles+1] = cpu.debug:bpset(0x29f92e, nil,
    'logerror "5510_records_word: value=%08x flash_header=%08x flash_tail=%08x observer_writes=0\\n",r0,d@200038,d@54fffc;g')
for _, item in ipairs({{0x29fdc4,'temp4','34'},{0x29ffe8,'temp5','35'}}) do
    local address, counter, command = item[1], item[2], item[3]
    handles[#handles+1] = cpu.debug:bpset(address, counter .. '<10',
        counter .. '=' .. counter .. '+1;logerror "5510_records_reply: command=' .. command ..
        ' ordinal=%d message=%08x state=%02x index=%02x observer_writes=0\\n",' ..
        counter .. ',r0,b@1204dd,b@1204e6;g')
end
local stop = emu.add_machine_stop_notifier(function()
    machine.debugger:command('logerror "5510_records_terminal: reply34=%d reply35=%d state=%02x index=%02x receipt34=%02x cap=16 observer_writes=0\\n",temp4,temp5,b@1204dd,b@1204e6,b@1204df')
end)
_G.noki5510_records_handles = {handles, stop}
