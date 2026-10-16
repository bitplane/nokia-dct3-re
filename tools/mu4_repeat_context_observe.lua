-- R060 bootrecstage bench only. Passive context observation; no DUT writes.
-- Retire after isolated saved-repeat coverage and original resumption proof.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'repeat context observation requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local debugger = assert(machine.debugger, 'repeat context observation requires -debug')
debugger:command('temp1=0;temp2=0;temp3=0')
debugger:command('bpset 7fffff,0')
debugger:command('bpset 3db4d,temp1==0,{temp1=1;printf "mu4_repeat_outer: sp=%04x st1=%04x brc=%04x rsa=%04x rea=%04x\\n",sp,st1,brc,rsa,rea;g}')
debugger:command('bpset 3206c,temp2<4&&rsa==db51&&rea==db56&&brc<40,{temp2=temp2+1;printf "mu4_repeat_vector: pc=%04x sp=%04x st1=%04x rtn=%04x brc=%04x\\n",pc,sp,st1,rtn,brc;history :cpu,40;g}')
debugger:command('bpset 3fa5e,temp3==0&&rsa==db51&&rea==db56,{temp3=1;printf "mu4_repeat_copy_entry: sp=%04x st1=%04x brc=%04x rsa=%04x rea=%04x\\n",sp,st1,brc,rsa,rea;history :cpu,80;g}')
local events, handles = {}, {}
local function record(kind, address, value)
    events[#events + 1] = string.format('mu4_repeat_context_%s: address=%04x value=%04x context_pc=%06x sp=%04x t=%.9f\n',
        kind, address, value, cpu.state['GENPC'].value, cpu.state['SP'].value, machine.time:as_double())
    if #events > 256 then table.remove(events, 1) end
end
for _, address in ipairs({7, 0x18, 0x1a, 0x1b, 0x1c}) do
    handles[#handles + 1] = cpu.spaces.data:install_write_tap(address, address, 'mu4_repeat_register_' .. address,
        function(at, value) record('write', at, value) end)
    if address ~= 0x18 then
        handles[#handles + 1] = cpu.spaces.data:install_read_tap(address, address, 'mu4_repeat_save_' .. address,
            function(at, value) record('read', at, value) end)
    end
end
local saved_words = {}
handles[#handles + 1] = cpu.spaces.data:install_write_tap(0x5cd3, 0x5cd6, 'mu4_repeat_saved_words', function(address, value)
    local pc = cpu.state['GENPC'].value & 0xffff
    if pc < 0x1ee0 or pc > 0x1f10 then return end
    saved_words[#saved_words + 1] = string.format('mu4_repeat_saved_word: address=%04x value=%04x context_pc=%06x\n',
        address, value, cpu.state['GENPC'].value)
    if #saved_words > 16 then table.remove(saved_words, 1) end
end)
-- Keep subscriptions alive for the complete observation window.
_G.mu4_repeat_context_handles = handles
emu.wait(5)
for _, event in ipairs(events) do machine:logerror(event) end
for _, event in ipairs(saved_words) do machine:logerror(event) end
for address = 0x5cd3, 0x5cd6 do
    machine:logerror(string.format('mu4_repeat_saved_terminal: address=%04x value=%04x\n', address, cpu.spaces.data:read_u16(address)))
end
machine:logerror(string.format('mu4_repeat_context_terminal: pc=%06x sp=%04x st1=%04x brc=%04x rsa=%04x rea=%04x observer_writes=0\n',
    cpu.state['GENPC'].value, cpu.state['SP'].value, cpu.state['ST1'].value, cpu.state['BRC'].value, cpu.state['RSA'].value, cpu.state['REA'].value))
for _, line in ipairs(debugger.consolelog) do print(line) end
machine:exit()
