-- Source text census for the idle radio-status overlay; observation only.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_menu_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local seen={}
_G.nsb7_display_text_tap=memory:install_read_tap(0x281568,0x28156b,'nsb7_display_text',function()
    if cpu.state['R15'].value~=0x28156a or cpu.state['R0'].value~=0x341420 or
            cpu.state['R6'].value~=0x1040e0 or cpu.state['R14'].value~=0x23ed71 then return end
    local time=machine.time:as_double()
    local second=math.floor(time)
    if second<14 or second>25 or seen[second] then return end
    seen[second]=true
    local words={}
    for i=0,79 do
        local word=memory:read_u16(0x1040e0+2*i)
        if word==0 then break end
        words[#words+1]=string.format('%04x',word)
    end
    machine:logerror(string.format('nsb7_display_text: font=00341420 pointer=001040e0 words=%s t=%.9f observer_writes=0\n',
        table.concat(words),time))
end)
