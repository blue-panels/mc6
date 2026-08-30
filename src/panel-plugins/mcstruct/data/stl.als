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
