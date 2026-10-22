_G.nhm3_fdn_record_steps={
    {42,'Scroll Down'},{43,'Scroll Down'},{46,'Left Softkey / Menu'},
    {48,'Scroll Down'},{50,'Left Softkey / Menu'},
    {53,'Keypad 5'},{54,'Keypad 6'},{55,'Keypad 7'},
    {56,_G.nhm3_fdn_record_wrong and 'Keypad 5' or 'Keypad 8'},
    {57,'Left Softkey / Menu'},
}
if not _G.nhm3_fdn_record_wrong then
    for _,step in ipairs({{60,'Keypad 1'},{61,'Keypad 2'},{62,'Keypad 3'},
            {63,'Keypad 4'},{64,'Keypad 5'},{65,'Left Softkey / Menu'},
            {68,'Keypad 2'},{71,'Left Softkey / Menu'}}) do
        table.insert(_G.nhm3_fdn_record_steps,step)
    end
end
_G.nhm3_fdn_record_finish=_G.nhm3_fdn_record_wrong and 64 or 78
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_fdn.lua')
