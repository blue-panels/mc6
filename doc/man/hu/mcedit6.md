---
date: 2026. szeptember
---

<!-- help:topics "Tartalomjegyzék" -->
# NÉV <!-- help:skip -->

mcedit6 - Belső fájlszerkesztő.

# ALKALMAZÁSA <!-- help:skip -->

**mcedit6**
[-bcCdfhstVx?] fájl

# Belső fájlszerkesztő <a id="internal-file-editor"></a>

A belső fájlszerkesztő egy teljes képernyős, minden szokásos eszközzel
ellátott szerkesztő. Az
*editor_filesize_threshold*
értéknél (alapértelmezésben 64 MB) nagyobb fájl kérdés után nyílik meg.
Bináris fájlokat is lehet vele szerkeszteni.
Az
**F4**
gomb indítja el, ha az inicializáló fájlban be van állítva a
*use_internal_edit*
opció.

A támogatott eszközök: blokk másolása, mozgatása, törlése, kivágása,
beillesztése; billentyűnkénti visszavonás; legördülő menük; fájl
beillesztése; makrók; keresés és csere reguláris kifejezéssel; szöveg
kijelölése Shift-kurzorral (ha a terminál ismeri); beszúrás-felülírás
váltása; sortörés; automatikus behúzás; állítható tabulátorméret;
szintaxiskiemelés többféle fájltípushoz; és szövegblokk átadása
shell-parancsnak, például az indent vagy az ispell programnak.

Szakaszok:
: [A szerkesztő beállításai](#editor-options)

A szerkesztő használata nagyon egyszerű és nem igényel magyarázatot. Hogy
melyik billentyű mit csinál, az a legördülő menükben látszik. Ezenkívül a
Shift és a kurzorbillentyűk szöveget jelölnek ki. A
**Ctrl-Ins**
a csereállományba másol
(**~/.local/share/mc6/mcedit/mcedit.clip**),
a
**Shift-Ins**
onnan illeszt be, a
**Shift-Del**
oda vág ki, a
**Ctrl-Del**
pedig törli a kijelölt szöveget. Az egérrel való kijelölés is működik, és a
szokásos módon megkerülhető: a Shift nyomva tartásával a terminál saját
egérkezelése marad érvényben.

Makró megadásához nyomd le a
**Ctrl-R**-t,
majd üsd le azokat a billentyűket, amelyeknek le kell futniuk. Ha kész vagy,
nyomd le újra a
**Ctrl-R**-t,
és rendeld a makrót egy billentyűhöz annak lenyomásával. A makró ezzel a
billentyűvel, valamint a
**Ctrl-A**
és a hozzárendelt billentyű leütésével fut le. A makrók a
**~/.local/share/mc6/macros**
fájl
**[editor]**
szakaszában vannak, és a hozzájuk tartozó sor törlésével szűnnek meg.

A megjelenített szöveg kódlapját az Alt-e (M-e) váltja. Az átkódolás a
választott kódlapról a rendszerére történik. Megszüntetéséhez a
kódlapválasztó ablakban a "\<No translation>" tételt kell választani.

A keresőablak
(**F7**)
**Szűrő**
gombja elrejti azokat a sorokat, amelyekben nincs benne a keresett szöveg,
ugyanazokkal a keresési, kis- és nagybetű, valamint teljes szó
beállításokkal, mint a keresés. A sorszámok az eredetiek maradnak, az
állapotoszlop pedig megjelöli minden elrejtett szakasz helyét. Az elrejtett
sorok köre a gomb megnyomásakor rögzül: a szerkesztés nem alkalmazza újra a
feltételt, így a kettévágott vagy összevont látható sor látható marad, és az
utána beírt sorok is láthatóak maradnak, akkor is, ha nem felelnek meg a
feltételnek. Az
**M-s**
leveszi a szűrőt; újra megnyomva az utolsó keresést teszi vissza szűrőként. A
Parancsok menü "Mindent kinyit" tétele szintén leveszi.

# A szerkesztő beállításai <a id="editor-options"></a>

A
[belső fájlszerkesztő](#internal-file-editor)
beállításai. A szerkesztő saját
**Beállítások**
menüje és a fájlkezelő Beállítások menüjének
**A szerkesztő beállításai**
pontja nyitja meg őket.

*Tördelési mód.*
Kikapcsolva, a bekezdés folyamatos formázása, vagy az írógépszerű tördelés,
amely gépelés közben töri a sort a megadott hossznál.

*Fél tabulátorok utánzása.*
A szöveg és a bal margó között a mozgás és a behúzás fél tabulátorral megy, és
szóközökkel telik ki; máshol a tabulátor a szokásos.

*Visszatörlés tabulátorokon át.*
Egyetlen visszatörlés a bal margóig törli a behúzást, ha a kurzor és a margó
között nincs szöveg.

*Tabulátorok kitöltése szóközökkel.*
Tabulátor karakter helyett szóközök kerülnek a következő tabulátorpozícióig.

*Tabulátor szélessége.*
Ennyi karakternek felel meg a tabulátor. Alapértelmezés szerint 8.

*Enter behúzást tart.*
Az új sor a fölötte lévő sor behúzásával kezdődik.

*Megerősítés mentés előtt.*
A fájl kiírása előtt kérdez.

*Pozíció megjegyzése.*
A fájl ott nyílik meg, ahol legutóbb elhagyták.

*Sorvégi szóközök mutatása.*
A sor végén lévő szóközök jelölve lesznek.

*Tabulátorok mutatása.*
A tabulátor karakterek jelölve lesznek.

*Vezérlőkarakterek mutatása.*
A szöveg vezérlőkarakterei kiíródnak, nem rejtve maradnak.

*Szintaxiskiemelés.*
A szöveg a fájltípusának szintaxisszabályaival színeződik.

*Kurzor a beszúrt blokk után.*
Blokk beszúrása után a kurzor a végén marad, nem az elején.

*Állandó kijelölés.*
A kijelölés a kurzor mozgatásakor megmarad, nem szűnik meg.

*Kurzor a sor vége mögött.*
A kurzor a sor utolsó karaktere mögött is állhat.

*Csoportos visszavonás.*
Egy visszavonás azonos fajtájú változtatások sorát vonja vissza, nem egyetlen
billentyűt.

*Tördelési sorhossz.*
Az az oszlop, amelynél a tördelési módok törik a sort. Alapértelmezés szerint
72.

# Mentés másként <a id="save-file-as"></a>

A név, amellyel a fájlt kiírjuk, és a sorvégek, amelyekkel kiírjuk: ahogy a
fájlban vannak, Unix (LF), Windows és DOS (CR LF) vagy Macintosh (CR).

# Mentési mód <a id="edit-save-mode"></a>

Hogyan íródik ki a fájl:

**Gyors mentés**
: Azonnal a fájlra ír. Gyors, és a közben bekövetkező hiba félig kiírt fájlt
hagy hátra.

**Biztonságos mentés**
: Előbb ideiglenes fájlba ír, és azt nevezi át az eredetire, amikor egészben
kész, így a hiba nem bántja az eredetit.

**Biztonsági másolat ezzel a kiterjesztéssel**
: Biztonságos mentés, és az eredeti megmarad a saját nevén a beviteli sorban
megadott kiterjesztéssel, alapértelmezés szerint "~".

**POSIX sorvég ellenőrzése**
: Kiírás előtt kérdez a fájl végéről hiányzó sorvégről.

# Makró böngésző <a id="macro-explorer"></a>

A felvett makrók, azzal a billentyűvel, amelyre mindegyik válaszol, és azzal,
amit csinál. A gombok:

**Futtatás**
: Lejátssza azt a makrót, amelyen a kurzor áll.

**Törlés**
: Kérdés után eltávolítja.

**Fájl szerkesztése**
: Megnyitja azt a fájlt, amelyben a makrók vannak.

# Megnyitott fájlok <a id="open-files"></a>

A szerkesztőben megnyitott fájlok, soronként egy. Az Enter arra a fájlra lép,
amelyen a kurzor áll, az Esc a megjelenítettet hagyja.

# Bővítmények adatai <a id="plugin-info"></a>

A szerkesztő által betöltött bővítmények: a név, be van-e kapcsolva, mit nyújt
és mit csinál. Ez néznivaló lista; kikapcsolni és beállítani a bővítményt a
fájlkezelő
[Bővítmények kezelése](mcommander.md#manage-plugins)
ablakában lehet.

# Lásd még... <a id="see-also"></a>

mcommander(1), mview(1).
