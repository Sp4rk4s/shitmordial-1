# Source font and texture dependencies

Static original-dump analysis2026-10-20. SHA256
e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768.
RVA addresses; analysis base44000000, runtime44340000. No live game calls/writes.

## Verified cached font constructor

Native entry35F040..35F258, runtime4469F040..4469F258. First32-byte and final8-byte
byte guards, complete-body digest and proposed ABI are in
font-render-annotations-pending.json. This start is absent from the retained IDA
function inventory, so it needs an explicit byte-guarded function definition.

Observed ABI: `unsigned int *__thiscall surface_font_construct_cached(
unsigned int *font,const char *face,unsigned int length,int tall,int weight,
int flags,int range_min,int range_max)`.
ECX=12-byte destination; seven stack arguments; RET1Ch; EAX=destination.

The48-byte cache records at runtime44B487D0..44B487D4 contain a24-byte MSVC face
string, five DWORD settings(tall,weight,flags,min,max), then the font handle at+2Ch.
An equal face (bounded byte lexical compare82DD0) and identical five settings
reuse the handle. A miss calls SourceSurface4481E15C virtual+11Ch(CreateFont),
then virtual+120h(SetFontGlyphSet) with(handle,face,tall,weight,0,0,flags,min,max).
The glyph setup BOOL is ignored. Name length is used by cache/string operations;
Surface receives a NUL-terminated face pointer, not an explicit face length.
The record is moved into the vector or added through its growth helper.

Both hit and miss paths call35FE60 twice. UTF16 `A` (length1) supplies destination
DWORD+4=width; UTF16 `ABCDEF` (length6) supplies destination DWORD+8=height.
Destination DWORD+0 is the font handle. Source Surface+13Ch is GetTextSize.

Additional REQUIRED dependency on a miss: global4481E1E8 must point to a valid
object with writable DWORD+28h. The constructor saves that field, replaces it
with pointer447940AC (`english`), calls glyph setup, then restores the field.
Its object identity and original initialization are NOT established. Neither
Surface nor this object has a safe null return path. Do not invoke this constructor
by only supplying guessed font arguments or binding Surface alone.

A full byte scan finds only one direct E8 caller:29E365. That surrounding body
uses Lua conversion helpers and passes the destination with a script-provided
face string, size/weight and parsed flags. This is a Lua-facing constructor lead,
not a proven main font initialization coordinator. Protected/indirect callers
remain unresolved. The language global has only two literal references in the
dump, both the save/restore above; no native direct initializer was recovered.
Cache pointer references outside this constructor are cleanup at40D180, not proof
of font creation during C++ constructors.

## Verified font measurement wrapper

35FE60..35FEDE, runtime4469FE60, thiscall destination-output/text/length stackargs;
RET0Ch, EAX=output int[2]. Length0 returns{0,0} without reading text; otherwise
passes font[0],NUL-terminated UTF16 text,&width,&height to Surface virtual+13Ch.
Length is not forwarded as a read bound. Seventy-one direct byte-scanned callers
include many UI/text drawing sites and the two font-constructor calls.

## Texture setup scope

35F260..35F2D9 is a no-argument texture creator: SourceSurface+ACh creates a
procedural texture; XOR-decoded `sprites/scope_arc` is passed to Surface+90h
DrawSetTextureFile with hardwareFilter1/forceReload0; EAX returns ID. Its only
direct caller35F8A9 is in a lazy shape/circle drawing routine. This is not a
confirmed menu font/texture initialization root.
Other consumers of Surface+94h/+98h supply RGBA upload/texture binding, including
Lua texture/image and general UI drawing functions. They do not establish a
shared startup sequence merely by using those methods.

Source method signatures were checked against Valve's public ISurface contract:
https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/public/vgui/ISurface.h
Exact payload argument pushes and observed virtual offsets are the local evidence;
the public SDK alone does not prove installed-layout equivalence.

## Persistence status

IDA GUI/RPC is unresponsive in the current session. No live/index application or
on-disk IDA save is claimed. Proposed names/prototypes/comments and exact guards
for the font constructor, measurement wrapper, relevant bounded comparison
helpers and scope texture creator are retained separately in
font-render-annotations-pending.json for reviewed merge/application once IDA
responds. Existing analyst edits must be preserved. No shared project-index write
was performed while this state is unresolved.

Evidence also includes surface-font-create-native.txt,font-related-35fe60.txt,
font-create-caller-leads.json,font-cache-and-language-native-references.json,
surface-font-texture-offset-candidates.json and surface-texture-35f260.txt.

## Follow-up: current FontManager identity and incompatible language access

Parent retained render-current-surface-font-methods-21728.json: current Surface
CreateFont71,SetFontGlyphSet72 andGetTextSize79 all dispatch through FontManager
singleton69AA5288. The old captured E1E8 value7B355268 has module RVA105268;
current manager RVA105288 differs20h, as does the old/current Surface singleton.
This strongly supports E1E8's role as a FontManager dependency, but is not a
license to rebind it using the old writable-field contract.

Passive read of current69AA5288+28h yields the INLINE bytes`english\0` followed
by zeros; its first DWORD6C676E65 is characters, not an established pointer.
Installed glyph-manager code confirms this:699B789A setsEBX=this;699B7919 and
subsequent language checks useLEA ECX,[EBX+28h], then pass that address directly to
case-insensitive byte comparison699C61B0 against language names (korean,schinese,
japanese,polish,etc.). The comparison starts with byte reads fromECX itself.

Thus copying current singleton69AA5288 intoE1E8 would make the payload constructor
write pointer bytesAC407944 over the first4 inline language bytes during glyph
setup. This is NOT a verified compatible binding/repair. Whether the old module
used a different language layout or the dump contains another altered assumption
remains unresolved. No target state was modified to test it.

Evidence surface-font-manager-language-21728.json,
current-font-glyph-manager-disassembly.txt,current-font-language-compare-prefix.txt.
The installed DLL disassembly uses actual runtime function locations with on-disk
absolute operands still at the DLL preferred base10000000; those absolute operands
must be adjusted only when comparing them to live addresses.
