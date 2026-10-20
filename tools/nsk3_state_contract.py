"""Own NSK-3 state1 candidate; not an exhaustive producer/writer census."""
from tools.nsk3_static_check import inspect


def decode(image):
    fragments={0x2536bc:'48b54285d101f000fed8',0x2544bc:'48eae683',
               0x219136:'486b2701750746417541',0x21a674:'1c282104f063fcfd',
               0x21a6a4:'78a8f7fefd25',0x20af86:'20a3010198004288d101f003fd20',
               0x20e9da:'48d1e00a',0x208df8:'484e7fc170a17f0070e0e052',
               0x295c22:'207446692202f010fae5',0x21ac36:'203f802820ff30778420',
               0x21ac9a:'210102494288db0849e04288da05',
               0x21d670:'8c2120ff30774281da4c',
               0x2881e0:'48821a082800d100e77038ff38a02800d100e760',
               0x2880b6:'78a02800d100e1024ec5e100',
               0x248616:'9d0420ff303b5d402800d10148dde00048dd1c29f7fefd37',
               0x248b9e:'9d0420ff303b5d402800d10148eae00048ea1c29f7fefa73',
               0x249292:'9d0420ff303b5d402800d101489ae000489a1c29f7fdfef9',
               0x24709c:'b5f01c0d1c06200cf020fbd01c04782870a08026',
               0x24753e:'f01ffb9f1c078838',0x2ab522:'20801a082800d050',
               0x281f16:'39102900d019',
               0x281f50:'4815464180084640790072c846407980730846402128f7fcf983200e4641e131',
               0x2820a2:'48ef464180084640790072c8',0x2820ce:'200e4641f7e4fcab',
               0x2539ba:'48da4285d100e174',0x253cbe:'48dd21002203f034f86e',
               0x288170:'2d02d1072e00d000e0ac485b68402800d000e0a62e00',
               0x288000:'6074e0ac',0x2557be:'7aa02802d0192803d017a070f028fa182002f032fbce',
               0x208a74:'48df1a282800d100e1fc38012800d100e21138012800d100e1da',
               0x208e44:'2e00d10b483a6d4160a16d407900084007400f402804d1052003e0041c30f7fffc5960a0200070a020017160e01b'}
    for address,encoded in fragments.items():
        expected=bytes.fromhex(encoded)
        if image[address-0x200000:address-0x200000+len(expected)]!=expected:
            raise ValueError('own state-transition instructions differ')
    def word(address):
        return int.from_bytes(image[address-0x200000:address-0x200000+4],'big')
    selector=word(((0x2536bc+4)&~3)+0x2d4)
    header=word(((0x2544bc+4)&~3)+0x3a8)
    owner=word(((0x219136+4)&~3)+0x1ac)
    if word(0x20ed20)!=0x834 or word(0x208f34)!=0x10a7d8:
        raise ValueError('upstream constructor header/context differs')
    if (selector,header,owner)!=(0x834,0x43f,0x106ce8):
        raise ValueError('own transition selector/header/owner differs')
    completion_input=word(0x2883ec)+0xff+0xa0
    if completion_input!=0x5e6 or word(0x2883d4)!=0x7e5:
        raise ValueError('helper completion namespace differs')
    if word(0x281fa8)!=0x40b or word(0x282460)!=0x40b:
        raise ValueError('lower decoder header namespace differs')
    if word(0x253d24)!=0x7ea or word(0x2883fc)!=0x7ea or word(0x2882e8)!=0x10a400:
        raise ValueError('queued-work selector/owner differs')
    if word(0x208df4)+2!=0x7d3:
        raise ValueError('queue constructor input arithmetic differs')
    return {'upstream_selector':0xa30,'upstream_context':0x10a7d8,
            'completion_input':completion_input,'nonzero_completion_result':0x7e5,
            'selector':selector,'header':header,'owner':owner,'new_state':1,
            'exhaustive_writer_census':False}


def inspect_source(source):
    _,image=inspect(source,'boot4')
    return decode(image)


def direct_call_candidates(image,target):
    """Decode aligned direct BL candidates without claiming code/data ownership."""
    from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN
    decoder=Cs(CS_ARCH_ARM,CS_MODE_THUMB|CS_MODE_BIG_ENDIAN)
    calls=[]
    for offset in range(0,len(image)-4,2):
        if int.from_bytes(image[offset:offset+2],'big')&0xf800!=0xf000:continue
        instructions=list(decoder.disasm(image[offset:offset+4],0x200000+offset))
        if not instructions or instructions[0].mnemonic!='bl' or instructions[0].op_str!=f'#{target:#x}':continue
        calls.append(0x200000+offset)
    return calls


def direct_server_sends(image):
    """Aligned direct BL candidates; only final-two-halfword literal destinations."""
    calls=direct_call_candidates(image,0x266a2c)
    selected=[]
    for address in calls:
        offset=address-0x200000
        if b'\x20\x0e' in (image[offset-2:offset],image[offset-4:offset-2]):
            selected.append(address)
    return {'direct_post_candidates':len(calls),'narrow_mailbox0e_sites':selected,
            'shared_indirect_computed_destinations_covered':False}
