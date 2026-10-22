-- Network-originated request, answered only through physical handset contacts.
_G.nsb7_ussd_snapshots={23}
_G.nsb7_ussd_actions={
    {24,'End'},
    {26,'Left Softkey / Menu'}, {28,'Left Softkey / Menu'},
    {30,'Keypad 2'}, {32,'Left Softkey / Menu'},
    {36,'End'}, {38,'Left Softkey / Menu'},
}
_G.nsb7_ussd_finish=41
_G.nsb7_ussd_response_times={25,27,29,31,33,37,39}
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'nsb7_host_ussd_observe.lua')
