---
date: 2026. szeptember
---

<!-- help:topics "Tartalomjegyzék" -->
# NÉV <!-- help:skip -->

mview - Belső fájlnéző.

# ALKALMAZÁSA <!-- help:skip -->

**mview**
[-bcCdfhstVx?] fájl

# Belső fájlnéző <a id="internal-file-viewer"></a>

A belső fájlnéző három megjelenítési módra képes: ASCII, hexadecimális és
szerkezeti (fa). Az ASCII és a hexadecimális mód között az F4 billentyű vált,
a szerkezeti módba az Alt-s vagy a t visz. ASCII módban az F9 ugyanazoknak a
bájtoknak három megjelenítését járja körbe: sima szöveg, a fájlban levő ANSI
színek, és egy terminálemulátor, amely megkapja a fájlt.

A fájlnéző megpróbálja a rendszer és a fájl típusa szerint a legjobb módot
használni a megjelenítésre.
Az nroff módban, amelyet a
[társítások fájlja](mcommander.md#edit-extension-file)
kapcsol be a kézikönyvlapokhoz, az előformázott lap átütéses sorozatai
vastagon és aláhúzva jelennek meg, nem pedig betű szerint.

ASCII módban a kurzorbillentyűk a képet mozgatják, amíg be nem kapcsol az
olvasókurzor. Az Enter bekapcsolja, és újra ki; bekapcsolja az egérkattintás
és a kijelölő billentyűk is. Bekapcsolt kurzorral a kurzorbillentyűk a
szövegben járnak, a Shift-kurzor, a Shift-Home, a Shift-End, a Shift-PageUp
és a Shift-PageDown pedig kijelölnek.

A bal egérgombbal húzva szöveget lehet kijelölni. A dupla kattintás egy szót,
a hármas egy képernyősort jelöl ki. Az egyszerű kattintás csak a kurzort
teszi egy karakterre: azt a pontot jelöli ki, ahonnan a Shift-es billentyűk a
kijelölést bővítik. Amikor a szöveg gördül (PageDown, egérgörgő), a kurzor a
képernyőn marad, ugyanabban a sorban és oszlopban. A Ctrl-Insert vagy az
Enter a kijelölést a csereállományba, onnan pedig a rendszer vágólapjára
másolja; a C-u megszünteti a kijelölést és kikapcsolja a kurzort, és ugyanezt
teszi a másolás is. Ahol nincs mit bekapcsolni, hexadecimális és fa módban,
ott az Enter úgy működik, mint a Down. A kurzor minden Shift nélküli mozgása
eldobja a kijelölést, a kurzort meghagyja. A másolt érték a megjelenített
szöveg: az ANSI és az nroff formázás lekerül róla, a tabulátorok szóközökké
válnak, szűrő módban pedig csak a látható sorok másolódnak.

Mivel a bal gomb szöveget jelöl ki, a kép felső vagy alsó harmadára
kattintva gördítés (lásd a
*mouse_move_pages*
beállítást a [Viewer] szakaszban) ASCII módban a jobb vagy a középső gombbal
történik. Az egérgörgő minden módban két sort gördít.

Az F6 úgy szűri a képet, ahogy a grep: csak a mintára illeszkedő sorok
látszanak, az állapotsor pedig számolja őket. A párbeszédablak a mintát kéri,
és ugyanazokat a keresési, kis- és nagybetű, valamint teljes szó
beállításokat használja, mint a keresés; az Ellenőrzés gomb kilistázza a
mintára illeszkedő sorokat még a szűrés előtt, az ott megnyomott F1 pedig
megnyitja a
[reguláris kifejezések gyors áttekintését](mcommander.md#regex-quick-reference).
Az üres minta törli a szűrőt, és amíg semmi nem illeszkedik, a kép ezt
kiírja, és ott marad, ahol volt. A ] és a [ billentyű a találatok között lép,
a C-t pedig bekapcsolja a követő módot, amelyben a kép az utolsó találaton
marad, miközben a fájl nő, akárcsak a tail -f esetében. A nagy fájl a
háttérben olvasódik be, amit az állapotsor jelez, amíg tart.

Hexadecimális módban a keresés idézőjeles szöveget és számokat is elfogad. Az
idézőjeles szöveget az idézőjelek nélkül keresi meg, minden szám egy bájtnak
felel meg. A kettőt keverni is lehet:

```
"Szöveg" 34 0xBB 012 "további szöveg"
```

A számok mindig hexadecimálisak. A fenti példában a "34" jelentése 0x34. A
"0x" előtag nem szükséges: "BB" is írható "0xBB" helyett. A "012" pedig 0x12,
nem pedig oktális szám.

A szerkezeti mód a JSON, a YAML és az XML fájlokat kinyitható faként
jeleníti meg; a HTML fájlt ugyanaz az elemző olvassa, mint az XML-t. A
formátumot a fájl neve, a file parancs válasza és maga a szöveg adja. A
támogatott fájlon az Alt-s (vagy a t) lép be a módba; ha a fájl nem
elemezhető, vagy nagyobb, mint amit a fa elbír, üzenet jelenik meg, a
fájlnéző pedig ASCII módban marad. A fa csak helyi fájlokkal működik, és a
belépés félreteszi a sorszűrőt és a terminál módot. A YAML-t beépített
elemző kezeli, amely a szokásos részhalmazt fedi le (blokkos leképezések és
sorozatok, blokkos skalárok, idézőjeles skalárok, horgonyok és hivatkozások).
A hivatkozásokat másolás bontja ki; a körkörös hivatkozás, vagy a belső
csomóponthatáron túli kibontás, másolat helyett \*név hivatkozásként jelenik
meg. A címkéket és a soron belüli gyűjteményeket ([] és {}) nem elemzi, azok
sima szöveges értékként látszanak; a részhalmazon kívüli dokumentumokat
(többsoros idézőjel nélküli skalárok, tabulátor a behúzásban) jelzi, és
szövegként mutatja. A fán belül az F4, az Alt-s és a t visszavisz ASCII
módba. Az állapotsor a tartalom típusát és az aktuális csomópont jq stílusú
útvonalát mutatja (például
**.spec.containers[0].image**).
A kattintás a fa kurzorát mozgatja, a kurzor sorára kattintva pedig kinyílik
vagy becsukódik az a csomópont.
A mód a gyorsnézet panelben (C-x q) is működik, ott a fa a panel kurzorát
követi. A [Viewer] szakasz
*structured_auto*
beállításával a támogatott fájlok rögtön a fában nyílnak meg. A fában
használható billentyűk:

```
Enter        csomópont kinyitása/becsukása; levélen a teljes
             érték megmutatása
Right/Left   kinyitás / becsukás (becsukott csomóponton a Left
             a szülőre ugrik)
*            az aktuális részfa teljes kinyitása
+ / -        az egész fa kinyitása / becsukása
1 .. 9       a dokumentum kinyitása a megadott mélységig
Alt-Enter    az aktuális csomópont útvonalának másolása
F7, /        keresés az egész dokumentumban, a becsukott
             csomópontokban is; a találat útvonala kinyílik
F17, n       a keresés folytatása
F6           a fa szűrése mintával
] / [        a következő / előző találatra lépés
```

A szűrő (F6) ugyanabban a párbeszédablakban kéri a mintát, mint az ASCII
módban, ugyanazokkal a beállításokkal, és csak azokat a csomópontokat hagyja
meg, amelyek kulcsa vagy értéke illeszkedik. A találatokhoz vezető útvonal
látható marad, és a találaton belüli rész is, így a találat kinyitható és
bejárható. Az állapotsor számolja a találatokat; a ] és a [ közöttük lép. Az
üres minta törli a szűrőt, és ugyanezt teszi a fából való kilépés is. A
csomópont a sorában látható szöveg szerint illeszkedik, így a hosszú érték
csak addig a hosszig, ameddig a fa az előnézetet tárolja (160 karakter).

A szövegben mozgató billentyűk a fában is mozgatnak: a kurzorbillentyűk és a
h, j, k, l, a lapozók, valamint az elejére és a végére vivők. Az Alt-e a
kódlapot választja, a C-o pedig a parancsképernyőt mutatja, akárcsak a
szövegben.

Ez a lista tartalmazza azokat a billentyűket, amelyekhez művelet kapcsolódik
a belső fájlnézőben.

**F1**
: Elindítja a beépített hypertext súgót.

**F2**
: Sortörés módot vált. Hexadecimális módban a bájtok szerkesztésére vált és
vissza; az F6 kiírja a változtatásokat a fájlba.

**F4**
: Hexadecimális módot vált.

**Alt-s, t**
: Szerkezeti (fa) módot vált a JSON, a YAML és az XML fájlokhoz.

**F14**
: Megmutatja a fájlt a szerkezetnézőben, az mcstruct bővítményben, azon a
bájton, amelyen a kurzor áll. Csak helyi fájlokkal működik.

**F6**
: Mintával szűri a képet. Hexadecimális módban menti a bájtokon végzett
változtatásokat.

**C-t**
: A szűrő követő módját kapcsolja.

**], [**
: A szűrő következő vagy előző találatára lép.

**F5**
: Ugrás. A párbeszédablak sorszámot, a fájl méretének százalékát, vagy tízes,
illetve tizenhatos számrendszerben írt pozíciót fogad el, aszerint hogy a
négy közül melyik van kiválasztva.

**F7, /, ?**
: Keresés indítása. Ezek a billentyűk a keresés beállításait tartalmazó
ablakot nyitják meg. A ? esetén a "Visszafelé" beállítás be van kapcsolva.

**C-s**
: A keresés folytatása előre.

**C-r**
: A keresés folytatása visszafelé.

**F17, n**
: A keresés folytatása a választott irányban.

**N**
: Egyszeri irányváltás: visszafelé keres, ha előre kereső van beállítva, és
fordítva.

**Shift-F8**
: Szintaxiskiemelést kapcsol: a szöveget a szerkesztő szintaxisszabályai
színezik, ugyanazok és ugyanúgy. 4 MB fölött a fájl a sorra korlátozott
szabályokat kapja, amelyek a számokat, az idézőjeles szövegeket és az
írásjeleket színezik anélkül, hogy a megjelenített sor fölötti egészet el
kellene olvasni. A szöveg megtartja a saját színeit, amíg az ANSI, a
hexadecimális vagy a terminál mód be van kapcsolva, és a szintaxisszabályok
ott félreállnak.

**Shift-F9**
: A szövegben levő ANSI színsorozatok értelmezését kapcsolja.

**F8**
: Vált a nyers és a feldolgozott mód között: a fájlt úgy mutatja, ahogy a
lemezen van, vagy, ha az extensions.ini fájlban meg van adva megjelenítő
szűrő, akkor a szűrő kimenetét. Az aktuális mód mindig a másik, mint amit a
gomb felirata mutat, mert a gombon az a mód áll, amelyikbe a billentyű visz.

**F9**
: A szöveg megjelenítési módjait járja körbe: sima, majd a fájl ANSI
színeivel, majd a terminál mód, amelyben a fájl egy terminálemulátorba megy,
és a fájlnéző az általa rajzolt képernyőt mutatja. A gomb felirata azt a
módot nevezi meg, amelyikbe a billentyű visz, nem azt, amelyikben a fájlnéző
van. A terminál mód a bájtfolyamból épül, nem a fájl soraiból, így ott a
sortörésnek, az ugrásnak, a szűrőnek és a keresésnek nincs min dolgoznia,
amit a gombsor ki is ír.

**F3, F10, Esc, q**
: Kilép a belső fájlnézőből.

**PageDown, szóköz, f, C-v**
: Egy lapot lapoz előre.

**PageUp, b, Alt-v, Backspace**
: Egy lapot lapoz vissza.

**d, u**
: Fél lapot lapoz előre vagy vissza.

**C-a, C-e**
: A sor elejére vagy végére lép.

**Home, C-Home, C-PageUp, g**
: A fájl elejére lép.

**End, C-End, C-PageDown, G**
: A fájl végére lép.

**Down, Up**
: A szövegkurzort egy sorral lejjebb vagy feljebb viszik; a kép szélén a
szöveg egy sort gördül. Hexadecimális módban egy sort gördítenek.

**Left, Right**
: A szövegkurzort egy megjelenített karakterrel mozgatják; a sor végén a
következőre lépnek. Hexadecimális módban a hexadecimális kurzort mozgatják.

**h, j, k, l**
: Balra, le, fel és jobbra mozgatnak, mint a kurzorbillentyűk. Az Insert és a
Delete, a C-p és a C-n, valamint az y és az e szintén egy sort mozgat fel és
le.

**Tab**
: Hexadecimális módban a hexadecimális és a szöveges oszlop között vált.

**C-Left, C-Right**
: A szövegkurzort nyolc megjelenített karakterrel mozgatják. Kikapcsolt
kurzorral és sortörés nélkül a képet tíz oszloppal gördítik oldalra.

**Shift-Left, Shift-Right, Shift-Up, Shift-Down**
: A kijelölést egy megjelenített karakterrel vagy sorral bővítik.

**Shift-Home, Shift-End**
: A kijelölést a képernyősor elejéig vagy végéig bővítik.

**Shift-PageUp, Shift-PageDown**
: A kijelölést az első vagy az utolsó látható sorig bővítik.

**Ctrl-Insert, Enter**
: A kijelölt szöveget a csereállományba és a rendszer vágólapjára másolják.
Kijelölés nélkül az Enter úgy működik, mint a Down.

**C-u**
: Megszünteti a szöveg kijelölését.

**C-l**
: Frissíti a képernyőt.

**C-o**
: Megmutatja a parancsképernyőt.

**[n] m**
: Beállítja az n jelölést.

**[n] r**
: Az n jelölésre ugrik.

**C-f**
: A következő fájlra ugrik. A gyorsnézet panelben nem, az a másik panel
kurzorát követi.

**C-b**
: Az előző fájlra ugrik. A gyorsnézet panelben ez sem.

**Alt-r**
: A vonalzót járja körbe: a kép tetején, az alján, és kikapcsolva.

**Alt-Shift-e**
: Megnyitja a korábban megnézett fájlok listáját, és a kiválasztottat mutatja
meg.

**Alt-e**
: A megjelenített szöveg kódlapját váltja. Az átkódolás a választott kódlapról
a rendszerére történik. Az átkódolás megszüntetéséhez a kódlapválasztó
ablakban a "\<No translation>" tételt kell választani.

Meg lehet tanítani a fájlnézőnek, hogyan jelenítsen meg egy fájlt, lásd a
[Társítások](mcommander.md#edit-extension-file)
részt.


# A megjelenítő beállításai <a id="viewer-options"></a>

A
[belső fájlnéző](#internal-file-viewer)
beállításai, amelyek minden megnyitott fájlra vonatkoznak.

*Hosszú sorok tördelése.*
Ha be van kapcsolva, a képernyőnél szélesebb sor a következő képernyősorban
folytatódik; különben levágódik, és a nézet oldalra gördül. Alapértelmezés
szerint be van kapcsolva.

*Szintaxiskiemelés.*
Ha be van kapcsolva, a megjelenítő a szerkesztő szintaxisszabályaival színezi
a szöveget. Az a fájl, amely saját színekkel érkező módban nyílik meg, például
egy kézikönyvlap vagy egy megformázott Markdown, megtartja azokat a színeket.
Alapértelmezés szerint ki van kapcsolva.

*Lapozás egérrel.*
Mennyit gördít a nézet felső vagy alsó harmadára kattintás: fél képernyőt, ha
be van kapcsolva, egy sort, ha nincs. A görgőt ez nem érinti, az mindig két
sort gördít. Alapértelmezés szerint be van kapcsolva.

*Pozíció megjegyzése.*
Ha be van kapcsolva, a fájl ott nyílik meg, ahol legutóbb elhagyták.
Alapértelmezés szerint ki van kapcsolva.

*JSON, YAML és XML fanézete.*
Ha be van kapcsolva, az ilyen fájlok rögtön összecsukható faként nyílnak meg,
nem sima szövegként. Ugyanez a nézet mindig elérhető a módot váltó
billentyűvel. Alapértelmezés szerint ki van kapcsolva.

*Fájl végének jelzése.*
Az a szöveg, amely a fájl utolsó sora után jelenik meg. Alapértelmezés szerint
üres.

*Legfeljebb ennyi újrarajzolás maradhat ki.*
Amíg a fájl olvasása tart, a megjelenítő újrarajzolásokat hagy ki, hogy lépést
tartson az adatokkal. Itt van megadva, hányat hagyhat ki egymás után.
Alapértelmezés szerint 10.

*A fanézet fájlméretkorlátja, MB.*
A legnagyobb fájl, amelyet a fanézet feldolgoz. A nagyobbat olvasás előtt
visszautasítja. Alapértelmezés szerint 64.

*A fanézet csomópontkorlátja.*
A legnagyobb fa, amelyet ez a mód felépít, csomópontokban. Alapértelmezés
szerint 10000000.

# Lásd még... <a id="see-also"></a>

mcommander(1), mcedit6(1).
