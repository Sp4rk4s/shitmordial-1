# Original native glow recovery (2026-10-21)

Read-only original-byte/current-game investigation; no production/shared-index/IDA modifications. Game11188 preserved. Actual gameplay glow is not claimed ready by these findings.

## Correct context and actual current manager

Original134980 loads DWORD[44819B7C+4650]=[4481E1CC], therefore payload RVA4DE1CC is GlowObjectManager. Calculation4500*4=4650 is exact. Earlier historical claims that4DE1CC is beams or4DE1B4 glow are incorrect; no production binding should use those labels.

Current client.dll unique reference lead `0F1105????????83C801` matches2C8F66 and writes singleton client+535FCB8. This match alone is not sole proof: current2C8F50 is an actual lazy static initializer/getter which zeroes vector, initializes free-list state and returns exactly535FCB8; actual ClientMode slot44 client+3FE450 calls2C8F50, then passes returnedmanager inECX to2C8FB0. That consumer obtains MaterialSystem115 rendercontext and invokes native glow rendering. Subsequent renderbody2C9410..2CBB30 reads manager array+0/count+0C; skips free entries unless firstDWORD==-2; uses entity+4; checksflags+28/+29/+2A; increments entrystride38 at2C9A25/2CA943/2CAA9A/2CB5BD. These independent original current-game uses prove manager/type/layout beyond a borrowed pattern.

Current11188 clientbase59590000 -> actualmanager5E8EFCB8; array732DDAB0,count66,allocation128. Read-only actualentries show existing CCSPlayer,classID40,CDynamicProp,weapon classes. For each readable entity+8networkable, vtable slot2 is MOV EAX,ClientClass;RET; class+8 string and+14ID verified. Capture `glow-current-manager-11188.json`; nativeconsumer text `glow-current-render-layout.txt`. All currentaddresses are observations; adapters need exact clientfileidentity andrelocation-aware originalmethodguards.

## Original hook and callable ABI

14FE60..14FE8D (2Dbytes) is a complete fastcall replacement with ECX receiver, EDX preserved and ONE stackarg, RET4. It saves receiver/EDX, calls original134980(noargs), forwards savedcallback7FB6B0 with original registers+stackarg, savesreturnedAL, calls134970(noargs), returns savedAL. Defensible prototype unsignedcharfastcall(void *clientmode,void *preserved_edx,void *viewsetup). Actual currentClientMode44 has precisely RET4 and passes stackarg into actualglowrender chain; currentmethod proof `glow-current-clientmode44-11188.json`. This is the original pre/post-glow wrapper; originalphase7DC938 brackets whole gameglow pass.

134970 is8-byte nativeclear: MOV BYTE[7DC938],0;RET.134980 is1C6byte noargcdecl routine, first sets7DC938=1, then iterates actualmanagerentryarray/count. It does not clearphase itself. Capsule/liveadapter must retain original wrapper call order including134970 aftersavedoriginal; do not leavephase1 betweenframes or activateit asfeaturetoggle.

Hashes:134980/1C6=87488cbff8f1670b54029562957eb7a19c9fd83f6d42664714375d76514d5bd8;134970/8=b83799c9076cb8306d79960f4ff6b4aa50c767147bbf8ae3a930bcaffee4b0d2;14FE60/2D=c922c7e75063e92a7deba5361be837e0bc785aee087beee798187faa4ce95f5a;F7050/6=1874c98cb56fea62939db1592fff9ce3c7d166ef86d734e0292b2092ce032b60.

## Original feature decisions/dependencies

134980 processes entries56bytes apart. It skips NULLentity+4 or nonplayerclass (networkable+8/slot2 ClientClass+14 comparedto57ED7C). For validplayers originalA06C0 local lookup chooses category0 forlocal; original36ACC0(entity,false) yieldscategory1enemy/2ally. Parent's already recovered native team/observer capsule provides realoriginal decisions; do not replacewithC++team logic.

Enablecontrols runtime4486EAA0+category13C =>RVA52EAA0/52EBDC/52ED18. Actualinventory originalcontrolbases52E968/52EAA4/52EBE0,label`glow`,value+138. Packed colors runtime4486EF78+category260 =>RVA52EF78/52F1D8/52F438. Originalcolors/categorization are not tobe guessed or forced.

If control disabled, onlycategory0 plus nativeF7050freecamactive takes built-inwhiteRGB1/alpha0.7 path. Otherwise disabled leavesentry untouched. Ifcontrolenabled, originalalpha0 leavesentryunchanged; positivealpha then local scoped&&native448755F8 equals the original constant atruntime44752710 (0.0) gate skipsnativewrite (rawUCOMISS+LAHF+JNP treats ordered equality; preservebytecode ratherthanreplacefloatcondition). Active path writes WORDentry+28=1 (occludedtrue/unoccludedfalse), byte+2A=0, nativepackedRGB/255 to+8/+C/+10,alpha/255 to+14. No customfeature formulas/calls needed.

Only old absolute Source operand in134980 is2DBDB5D3 supplying actuallocalm_bIsScoped; parent already independentlyverified current9974. Repair that operand toownedcurrentoffsetcell in guardedcopy, plusrel32calls to actualoriginal helper capsules (A06C0 and36ACC0) ifcopyrelocated. Remaining globals are actualpayloadUI/nativephase/context/classID/constants. Rawfullfunction disassembly `glow-0x44134980-asm.json`.

F7050 isclosed6-byte MOV AL,[7DBBA0];RET, notexternalfreecamreplacement. ItsoriginalwriterF7780 reads actualfreecamkey54A800slot8; active transition capturesview/localvelocity, setsnative7DBBA0, and runs originalfreecamF7060; inactive transition restoresoriginalview and clearsflag. This proves getter'sfreecamactive role; fullfreecamfeature is NOT restored byusingitsactualflag. Keepnativeflag unchanged; currentdefault0. Do not inventfreecam activation.

## Minimal defensible restoration path

Useactualcurrent manager object4DE1CC, realplayerclassID40 at57ED7C, alreadyclosedoriginal local/team/resource/observer helpers, scopedoperandrepair andactualUIstate. Full134980 can execute underguardedactualdependencies withoutnewglowalgorithm. Native14FE60wrapper belongs currentClientMode44 andsavedoriginal7FB6B0; MinHook ABIadaptermay publishverifiedsavedpointer andbindcontexts,butcolor/team/enablewrites mustremainnative. Beforeliveuse, isolatedtests mustexecuteactualoriginalblock coveringNULLentry/nonplayer/local/enemy/ally/off/alpha0/scoped/freecam andphasecleanup. No isolatedtestfixture may be usedlive.
