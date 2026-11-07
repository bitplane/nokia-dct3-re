-- Read-only native entry/return snapshots for isolated receive-kernel tests.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_native_observe.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
machine.debugger.visible_cpu=cpu
machine.debugger:command('temp5=0')
local fields={'pc','xpc','a','b','t','trn','sp','st0','st1','pmst','bk','brc','rsa','rea',
    'ar0','ar1','ar2','ar3','ar4','ar5','ar6','ar7'}
for index,entry in ipairs({{0x9268,'decode'},{0x4bbc,'decoded'},{0x4bc1,'crc_input'},{0x4bc5,'checked'}}) do
    local bit=1<<(index-1)
    local format,args={},{}
    for _,field in ipairs(fields) do
        format[#format+1]=field..'=%010x'
        args[#args+1]=field
    end
    local action=string.format('temp5=temp5|%x;save rom6_sch_%s_program.le16,0,10000;saved rom6_sch_%s_data.le16,0,10000;logerror "rom6_receive_entry: stage=%s %s observer_writes=0\\n",%s;g',
        bit,entry[2],entry[2],entry[2],table.concat(format,' '),table.concat(args,','))
    cpu.debug:bpset(entry[1],string.format('(temp5&%x)==0',bit),action)
end
_G.rom6_receive_stop=emu.add_machine_stop_notifier(function()
    machine.debugger.visible_cpu=cpu
    machine.debugger:command('logerror "rom6_receive_entries_complete: stages=%x expected=f observer_writes=0\\n",temp5')
end)
