-- Physical settings reset after own authentication; observations never write RAM.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
_G.nsm1_toolkit_observe_only=true
dofile(directory..'nsm1_toolkit_input.lua')
local machine=manager.machine
local memory=machine.devices[':maincpu'].spaces.program
local index=0
local function key(column,mask)
    local row=0
    while (1<<row)~=mask do row=row+1 end
    local code=memory:read_u8(0x2e0b18+row*5+column)
    index=index+1
    for _,field in pairs(machine.ioport.ports[':COL.'..column].fields) do
        if field.mask==mask then
            for _,down in ipairs({1,0}) do
                field:set_value(down)
                machine:logerror(string.format('nsm1_factory_contact: index=%d key=%02x column=%d mask=%02x down=%d t=%.9f physical_only=1\n',index,code,column,mask,down,machine.time:as_double()))
                if down==1 then assert(emu.wait(.2)) end
            end
            return
        end
    end
    error('own physical contact unavailable')
end
local actions=coroutine.create(function()
    assert(emu.wait(25))
    key(1,2)
    assert(emu.wait(1))
    machine.screens[':screen']:snapshot('factory_menu.png')
    key(2,4)
    assert(emu.wait(1))
    machine.screens[':screen']:snapshot('factory_settings.png')
    key(1,2)
    assert(emu.wait(1))
    machine.screens[':screen']:snapshot('factory_settings_open.png')
    key(4,4)
    assert(emu.wait(1))
    machine.screens[':screen']:snapshot('factory_restore.png')
    key(1,2)
    assert(emu.wait(1))
    machine.screens[':screen']:snapshot('factory_prompt.png')
    assert(emu.wait(2))
    for _,cell in ipairs({{2,2},{3,2},{4,2},{2,4},{3,4}}) do
        key(cell[1],cell[2])
        assert(emu.wait(.5))
    end
    machine.screens[':screen']:snapshot('factory_code.png')
    key(1,2)
    assert(emu.wait(5))
    machine.screens[':screen']:snapshot('factory_result.png')
end)
assert(coroutine.resume(actions))
machine.debugger:command('bpset 2920b0,1,{logerror "nsm1_factory_initializer: caller=%x cycles=%016X observer_writes=0\\n",r14,totalcycles;g}')
