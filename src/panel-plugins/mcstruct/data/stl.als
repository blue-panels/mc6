; def-file aliases: DEF.STL: name patterns and @offset:hexbytes signatures
exe.stl: *.dll *.drv *.ocx *.scr *.cpl @0:4D5A
zip.stl: *.jar *.apk *.docx *.xlsx *.odt @0:504B0304
elf.stl: *.so *.o @0:7F454C46
png.stl: @0:89504E470D0A1A0A
bmp.stl: @0:424D
wav.stl: @8:57415645
uimage.stl: @0:27051956
dtb.stl: @0:D00DFEED
mbr.stl: *.img @510:55AA
sqlite.stl: *.sqlite *.sqlite3 @0:53514C69746520666F726D6174203300
gif.stl: @0:474946
ico.stl: *.cur @0:00000100 @0:00000200
gzip.stl: *.tgz @0:1F8B
pcap.stl: *.pcapdump @0:D4C3B2A1 @0:A1B2C3D4 @0:4D3CB2A1 @0:A1B23C4D
cpio_old.stl: @0:C771
tga.stl: *.icb *.vda *.vst
pcx.stl: @0:0A
au.stl: *.snd @0:2E736E64
ines.stl: @0:4E45531A
gpt.stl: @512:4546492050415254
edid.stl: *.bin @0:00FFFFFFFFFFFF00
id3v1.stl: *.mp3
wad.stl: @1:574144
pak.stl: @0:5041434B
mo.stl: *.gmo @0:DE120495 @0:950412DE
utmp.stl: *.wtmp *.btmp
midi.stl: *.mid *.midi *.smf @0:4D546864
cramfs.stl: @0:453DCD28
lzh.stl: *.lha @2:2D6C68
voc.stl: @0:437265617469766520566F6963652046696C65
ivf.stl: @0:444B4946
tim.stl: @0:10000000
xwd.stl: *.xwd
mdl.stl: @0:4944504F
trx.stl: @0:48445230
sparse.stl: *.simg @0:3AFF26ED
android_img.stl: *.img @0:414E44524F494421
hashcat_restore.stl: *.restore
aix_utmp.stl:
ether.stl: *.eth *.frame
dns.stl: *.dns
ssh_pubkey.stl: *.sshkey
homm_bmp.stl:
respack.stl: @0:5253
andes_fw.stl:
dcx.stl: @0:B168DE3A
chg.stl: *.chg
ftl_dat.stl:
nanoapp.stl: *.napp @4:4E414E4F
homm_agg.stl: *.agg
systemtime.stl:
dune2_pak.stl:
macho_fat.stl: @0:CAFEBABE
gt_vol.stl: *.vol @0:47544653
hccap.stl: *.hccap
stl3d.stl:
asus_bootldr.stl: @0:424F4F544C445221
fallout_dat.stl:
fallout2_dat.stl:
avantes_roh60.stl: *.roh
glb.stl: @0:676C5446
rtp.stl: *.rtp
heaps_pak.stl: @0:50414B
zisofs.stl: @0:37E45396C9DBD607
icmp.stl: *.icmp
android_dto.stl: *.dtbo @0:D7B7AB1E
luks.stl: @0:4C554B53BABE
glshaders.stl: @0:45474C24
amlogic_mpt.stl: @0:4D505400
shx.stl: @0:0000270A
tsm.stl: *.tsm @0:16D116D1
op2.stl: *.op2 @0:234F504C5F494923
vmdk.stl: @0:4B444D56
zx_tap.stl: *.tap
shell_items.stl:
nt_mdt_pal.stl: *.pal @0:4E542D4D44542050616C6574746520
vpp.stl: *.vpp_pc @0:CE0A895104
websocket.stl:
hccapx.stl: *.hccapx @0:48435058
huawei_bootldr.stl: @0:3CD61ACE
apm.stl: @512:504D
ogg.stl: *.oga *.ogv *.opus *.spx @0:4F676753
chrome_pak.stl: *.pak
dime.stl:
sudoers_ts.stl:
mar.stl: @0:4D415231
xar.stl: *.pkg @0:78617221
gbr.stl: @20:47494D50
applesd.stl: @0:00051600 @0:00051607
tls_hello.stl:
pff2.stl: *.pf2 @0:46494C4500000004504646
id3v2.stl: @0:494433
cfb.stl: *.doc *.xls *.ppt *.msi *.msg @0:D0CF11E0A1B11AE1
bitcoin_tx.stl:
avi.stl: @8:41564920
winres.stl: *.res
trdos.stl: *.trd
efi_siglist.stl:
pif.stl: @0:50494600
lvm2.stl: @512:4C4142454C4F4E45
rar.stl: @0:526172211A07
btrfs_stream.stl: @0:62747266732D73747265616D00
allegro_dat.stl: @4:414C4C2E
qcom_bootldr.stl:
blend.stl: @0:424C454E444552
vox.stl: @0:564F5820
android_super.stl: @4096:67446C61
journal.stl: *.journal @0:4C504B5348485248
swf.stl: @1:5753
uefi_te.stl: *.efi @0:565A
vdi.stl: @64:7F10DABE
lnk.stl: @0:4C0000000114020000000000C000000000000046
wmf.stl: @0:D7CDC69A
ext2.stl: @1080:53EF
iso9660.stl: @32769:4344303031
regf.stl: @0:72656766
rtcp.stl:
pcf.stl: *.pcf @0:01666370
ppi.stl:
pyc27.stl: *.pyc @0:03F30D0A
md2.stl: @0:49445032
xm.stl: @0:457874656E646564204D6F64756C653A20
protobuf.stl: *.pb
bson.stl: *.bson
asn1_der.stl: *.der *.cer *.crt
some_ip.stl:
specpr.stl:
mcap.stl: @0:894D43415030
minidump.stl: *.dmp *.mdmp @0:4D444D5093A7
rsrc.stl: *.rsrc
