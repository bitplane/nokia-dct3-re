local source=debug.getinfo(1,'S').source:sub(2)
local directory=source:match('^(.*[/\\])')
dofile(directory .. 'nsk3_native_upload_observe.lua')
dofile(directory .. 'dct3_native_program_write_census.lua')('nsk3')
local captured=false
emu.register_frame_done(function()
    if captured or manager.machine.time:as_double()<1 then return end
    captured=true
    manager.machine.debugger:command('dasm nsk3_program_high_early.asm,f00,30,1,:dsp_c54x:cpu')
    manager.machine.debugger:command('dasm nsk3_program_high_late.asm,49f0,40,1,:dsp_c54x:cpu')
end,'NSK-3 high program-write caller disassembly')
