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
