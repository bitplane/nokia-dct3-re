-- Install immediately: headless debugger startup files can remain queued.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_fdn_dial.lua')
local machine=manager.machine
local upload_count,upload_highest=0,0
_G.nsk1_native_upload_tap=machine.devices[':dsp_c54x:cpu'].spaces['program']:install_write_tap(0x2800,0x2fff,'nsk1_native_upload_extent',function(offset,data,mask)
    upload_count=upload_count+1
    upload_highest=math.max(upload_highest,offset)
    if offset>=0x29d3 and offset<=0x29d7 then
        machine:logerror(string.format('nsk1_program_extent: address=%04x data=%04x mask=%04x t=%.9f observer_writes=0\n',offset,data,mask,machine.time:as_double()))
    end
end)
machine.debugger.visible_cpu=machine.devices[':dsp_c54x:cpu']
local initial_count=0
for _ in pairs(machine.devices[':dsp_c54x:cpu'].debug:bplist()) do initial_count=initial_count+1 end
local file=assert(io.open('native_entry.cmd','r'))
for line in file:lines() do machine.debugger:command(line) end
file:close()
machine.debugger:command('observe :maincpu,:dsp_c54x:cpu')
local count=0
for _ in pairs(machine.devices[':dsp_c54x:cpu'].debug:bplist()) do count=count+1 end
if count-initial_count~=128 then
    local console=machine.debugger.consolelog
    for i=1,#console do
        local line=console[i]
        if line then machine:logerror('nsk1_zero_debugger: '..line..'\n') end
    end
end
assert(count-initial_count==128,'all XPC-page breakpoints must install')
machine:logerror(string.format('nsk1_zero_setup: breakpoints=%d existing=%d t=%.9f observer_writes=0\n',count-initial_count,initial_count,machine.time:as_double()))
local captured=false
emu.register_frame_done(function()
    if captured or machine.time:as_double()<1 then return end
    captured=true
    machine:logerror(string.format('nsk1_program_extent_summary: writes=%d highest=%04x observer_writes=0\n',upload_count,upload_highest))
    machine.debugger:command('dasm native_entry_callers.asm,3700,300,1,:dsp_c54x:cpu')
    machine.debugger:command('dasm native_entry_target.asm,29b0,60,1,:dsp_c54x:cpu')
    machine.debugger:command('dasm native_entry_rom.asm,a500,200,1,:dsp_c54x:cpu')
end,'NSK-1 read-only native entry disassembly')
