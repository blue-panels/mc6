---
date: wrzesień 2026
---

# NAZWA <!-- help:skip -->

mview - Wbudowany podgląd plików.

# UŻYTKOWANIE <!-- help:skip -->

**mview**
[-bcCdfhstVx?] plik

# Wbudowany podgląd plików <a id="internal-file-viewer"></a>

Wbudowany podgląd plików ma trzy tryby wyświetlania: ASCII, szesnastkowy i
strukturalny (drzewo). Między trybem ASCII a szesnastkowym przełącza klawisz
F4, do trybu strukturalnego wchodzi Alt-s albo t. W trybie ASCII F9 obchodzi
trzy sposoby pokazania tych samych bajtów: zwykły tekst, kolory ANSI zawarte
w pliku oraz emulator terminala, któremu plik jest podawany.

Podgląd plików próbuje użyć najlepszej metody, jaką daje system albo typ
pliku.
W trybie nroff, który
[plik rozszerzeń](mcommander.md#edit-extension-file)
włącza dla stron podręcznika, ciągi nadpisania sformatowanej strony są
pokazywane pogrubieniem i podkreśleniem, a nie dosłownie.

W trybie ASCII klawisze strzałek przesuwają widok, dopóki nie włączy się
kursor czytania. Enter włącza go i wyłącza z powrotem; włącza go też
kliknięcie oraz klawisze zaznaczające. Przy włączonym kursorze strzałki
chodzą po tekście, a Shift-strzałka, Shift-Home, Shift-End, Shift-PageUp i
Shift-PageDown zaznaczają.

Przeciąganie lewym przyciskiem myszy zaznacza tekst. Podwójne kliknięcie
zaznacza słowo, potrójne wiersz ekranu. Pojedyncze kliknięcie tylko stawia
kursor na znaku: wyznacza punkt, od którego klawisze z Shiftem rozszerzają
zaznaczenie. Kiedy tekst się przewija (PageDown, kółko myszy), kursor
zostaje na ekranie, w tym samym wierszu i kolumnie. Ctrl-Insert albo Enter
kopiują zaznaczenie do pliku wymiany, a stamtąd do schowka systemu; C-u
zdejmuje zaznaczenie i wyłącza kursor, to samo robi kopiowanie. Tam, gdzie
nie ma kursora do włączenia, w trybie szesnastkowym i w drzewie, Enter działa
jak Down. Każdy ruch kursora bez Shiftu gubi zaznaczenie, a kursor zostaje.
Kopiowany jest tekst pokazany: formatowanie ANSI i nroff znika, tabulacje
zamieniają się w spacje, a w trybie filtra kopiowane są tylko widoczne
wiersze.

Ponieważ lewy przycisk zaznacza tekst, przewijanie kliknięciem w górną albo
dolną trzecią część widoku (zobacz
*mouse_move_pages*
w sekcji [Viewer]) w trybie ASCII robi się prawym albo środkowym
przyciskiem. Kółko myszy przewija po dwa wiersze, w każdym trybie.

F6 filtruje widok tak jak grep: widać tylko wiersze pasujące do wzorca, a
wiersz stanu je liczy. Okno pyta o wzorzec i o te same ustawienia rodzaju
szukania, wielkości liter i całych słów co szukanie; przycisk Sprawdź
wypisuje wiersze, które wzorzec wybiera, zanim filtr zostanie nałożony, a F1
otwiera tam
[krótki przegląd wyrażeń regularnych](mcommander.md#regex-quick-reference).
Pusty wzorzec zdejmuje filtr, a dopóki nic nie pasuje, widok to pisze i
zostaje tam, gdzie był. Klawisze ] i [ chodzą po trafieniach, a C-t włącza
tryb podążania, w którym widok zostaje na ostatnim trafieniu, podczas gdy
plik rośnie, tak jak robi to tail -f. Duży plik jest czytany w tle, o czym
wiersz stanu informuje przez cały czas.

W trybie szesnastkowym funkcja szukania przyjmuje tekst w cudzysłowach i
liczby. Tekst w cudzysłowach jest szukany dosłownie, bez cudzysłowów. Każda
liczba odpowiada jednemu bajtowi. Jedno z drugim można mieszać:

```
"Ciąg" 34 0xBB 012 "więcej tekstu"
```

Liczby są zawsze szesnastkowe. W przykładzie "34" znaczy 0x34. Przedrostek
"0x" nie jest potrzebny: można napisać "BB" zamiast "0xBB". A "012" to 0x12,
nie liczba ósemkowa.

Tryb strukturalny pokazuje pliki JSON, YAML i XML jako rozwijane drzewo;
plik HTML czyta ten sam analizator co XML. Format bierze się z nazwy pliku, z
tego, co mówi o nim polecenie file, i z samego tekstu. Alt-s (albo t) na
obsługiwanym pliku wchodzi do tego trybu; jeśli pliku nie da się
przeanalizować albo jest większy, niż widok drzewa przyjmuje, pokazuje się
komunikat, a podgląd zostaje w trybie ASCII. Drzewo działa tylko na plikach
lokalnych, a wejście do niego odkłada na bok filtr wierszy i tryb terminala.
YAML obsługuje wbudowany analizator, który pokrywa zwykle używany podzbiór
(odwzorowania i ciągi blokowe, skalary blokowe, skalary w cudzysłowach,
kotwice i aliasy). Aliasy są rozwijane przez kopiowanie; alias cykliczny albo
rozwinięcie ponad wewnętrzny limit węzłów pokazuje się jako odwołanie \*nazwa
zamiast kopii. Znaczniki i kolekcje w linii ([] i {}) nie są analizowane i
pojawiają się jako zwykłe wartości tekstowe; dokumenty spoza tego podzbioru
(wielowierszowe skalary bez cudzysłowów, tabulacje we wcięciu) są zgłaszane i
pokazywane jako tekst. Wewnątrz drzewa F4, Alt-s i t wracają do trybu ASCII.
Wiersz stanu pokazuje typ zawartości i ścieżkę bieżącego węzła w stylu jq (na
przykład
**.spec.containers[0].image**).
Kliknięcie przesuwa kursor drzewa, a kliknięcie w wiersz, na którym stoi
kursor, rozwija albo zwija ten węzeł.
Tryb działa też w panelu szybkiego podglądu (C-x q), gdzie drzewo podąża za
kursorem panelu. Przy włączonej opcji
*structured_auto*
sekcji [Viewer] obsługiwane pliki otwierają się od razu w drzewie. Klawisze
dostępne w drzewie:

```
Enter        rozwija/zwija bieżący węzeł; na liściu pokazuje
             pełną wartość
Right/Left   rozwija / zwija (na zwiniętym węźle Left skacze
             do rodzica)
*            rozwija całe bieżące poddrzewo
+ / -        rozwija / zwija całe drzewo
1 .. 9       rozwija dokument do zadanej głębokości
Alt-Enter    kopiuje ścieżkę bieżącego węzła do schowka
F7, /        szuka w całym dokumencie, także w zwiniętych
             węzłach; ścieżka do trafienia się rozwija
F17, n       szuka dalej
F6           filtruje drzewo wzorcem
] / [        idzie do następnego / poprzedniego trafienia
```

Filtr (F6) pyta o wzorzec w tym samym oknie co w trybie ASCII, z tymi samymi
ustawieniami rodzaju, wielkości liter i całych słów, i zostawia tylko te
węzły, których klucz albo wartość pasuje. Ścieżka do każdego trafienia
zostaje widoczna, podobnie jak to, co jest wewnątrz trafienia, więc
znaleziony węzeł można otworzyć i przejrzeć. Wiersz stanu liczy trafienia; ]
i [ po nich chodzą. Pusty wzorzec zdejmuje filtr, tak samo jak wyjście z
drzewa. Węzeł pasuje po tekście, który pokazuje jego wiersz, więc długa
wartość pasuje tylko do tej długości, którą drzewo trzyma na podgląd (160
znaków).

Klawisze, które poruszają się po tekście, poruszają się też po drzewie:
strzałki oraz h, j, k i l, klawisze stron i te, które idą na początek i na
koniec. Alt-e wybiera zestaw znaków, a C-o pokazuje ekran poleceń, tak samo
jak w tekście.

Oto lista działań przypisanych klawiszom we wbudowanym podglądzie plików.

**F1**
: Wywołuje wbudowaną przeglądarkę pomocy.

**F2**
: Przełącza tryb zawijania. W trybie szesnastkowym przechodzi do edycji
bajtów i z powrotem; F6 zapisuje zmiany do pliku.

**F4**
: Przełącza tryb szesnastkowy.

**Alt-s, t**
: Przełącza tryb strukturalny (drzewo) dla plików JSON, YAML i XML.

**F14**
: Pokazuje plik w podglądzie struktur, wtyczce mcstruct, na bajcie, na którym
stoi kursor. Działa tylko na plikach lokalnych.

**F6**
: Filtruje widok wzorcem. W trybie szesnastkowym zapisuje zmiany w bajtach.

**C-t**
: Przełącza tryb podążania filtra.

**], [**
: Idzie do następnego albo poprzedniego trafienia filtra.

**F5**
: Idź do. Okno przyjmuje numer wiersza, procent rozmiaru pliku albo pozycję
zapisaną dziesiętnie lub szesnastkowo, zależnie od tego, które z czterech
jest wybrane.

**F7, /, ?**
: Zaczyna szukanie. Te klawisze otwierają okno z ustawieniami szukania. Przy
klawiszu ? opcja "Wstecz" jest włączona.

**C-s**
: Szuka dalej w przód.

**C-r**
: Szuka dalej wstecz.

**F17, n**
: Szuka dalej w wybranym kierunku.

**N**
: Raz odwraca kierunek szukania: wstecz, jeśli wybrane jest szukanie w przód,
i na odwrót.

**Shift-F8**
: Przełącza podświetlanie składni: tekst jest kolorowany regułami składni,
tymi samymi i tak samo jak we wbudowanym edytorze. Powyżej 4 MB plik
dostaje reguły lokalne dla wiersza, które kolorują liczby, ciągi w
cudzysłowach i znaki przestankowe, bez czytania wszystkiego powyżej
pokazywanego wiersza. Tekst zachowuje własne kolory, dopóki włączony jest
tryb ANSI, szesnastkowy albo terminala, a reguły składni ustępują tam miejsca.

**Shift-F9**
: Przełącza interpretację ciągów kolorów ANSI zawartych w tekście.

**F8**
: Przełącza tryb surowy i przetworzony: pokazuje plik taki, jaki jest na
dysku, albo, jeśli w pliku extensions.ini podano filtr wyświetlania, wyjście
tego filtra. Bieżący tryb jest zawsze inny niż napis na przycisku, bo na
przycisku jest tryb, do którego ten klawisz prowadzi.

**F9**
: Obchodzi sposoby pokazywania tekstu: zwykły, potem z kolorami ANSI pliku,
potem tryb terminala, w którym plik trafia do emulatora terminala, a podgląd
pokazuje ekran, który ten rysuje. Napis na przycisku nazywa tryb, do którego
klawisz prowadzi, a nie ten, w którym podgląd jest. Tryb terminala powstaje ze
strumienia bajtów, a nie z wierszy pliku, więc zawijanie, skok do wiersza,
filtr i szukanie nie mają tam na czym pracować, o czym pasek przycisków
informuje.

**F3, F10, Esc, q**
: Wychodzi z wbudowanego podglądu plików.

**PageDown, spacja, f, C-v**
: Przewija o stronę w przód.

**PageUp, b, Alt-v, Backspace**
: Przewija o stronę wstecz.

**d, u**
: Przewija o pół strony w przód albo wstecz.

**C-a, C-e**
: Idzie na początek albo na koniec wiersza.

**Home, C-Home, C-PageUp, g**
: Idzie na początek pliku.

**End, C-End, C-PageDown, G**
: Idzie na koniec pliku.

**Down, Up**
: Przesuwają kursor tekstu o wiersz w dół albo w górę; na brzegu widoku tekst
przewija się o wiersz. W trybie szesnastkowym przewijają o wiersz.

**Left, Right**
: Przesuwają kursor tekstu o jeden pokazany znak; na końcu wiersza przechodzą
do następnego. W trybie szesnastkowym przesuwają kursor szesnastkowy.

**h, j, k, l**
: Przesuwają w lewo, w dół, w górę i w prawo, jak strzałki. Klawisze Insert i
Delete, C-p i C-n oraz y i e też przesuwają o wiersz w górę i w dół.

**Tab**
: W trybie szesnastkowym przełącza między kolumną szesnastkową a tekstową.

**C-Left, C-Right**
: Przesuwają kursor tekstu o osiem pokazanych znaków. Przy wyłączonym
kursorze i bez zawijania przewijają widok o dziesięć kolumn w bok.

**Shift-Left, Shift-Right, Shift-Up, Shift-Down**
: Rozszerzają zaznaczenie o jeden pokazany znak albo wiersz.

**Shift-Home, Shift-End**
: Rozszerzają zaznaczenie do początku albo końca wiersza ekranu.

**Shift-PageUp, Shift-PageDown**
: Rozszerzają zaznaczenie do pierwszego albo ostatniego widocznego wiersza.

**Ctrl-Insert, Enter**
: Kopiują zaznaczony tekst do pliku wymiany i do schowka systemu. Bez
zaznaczenia Enter działa jak Down.

**C-u**
: Zdejmuje zaznaczenie tekstu.

**C-l**
: Odświeża ekran.

**C-o**
: Pokazuje ekran poleceń.

**[n] m**
: Ustawia znacznik n.

**[n] r**
: Skacze do znacznika n.

**C-f**
: Skacze do następnego pliku. Nie w panelu szybkiego podglądu, który podąża
za kursorem drugiego panelu.

**C-b**
: Skacze do poprzedniego pliku. W panelu szybkiego podglądu również nie.

**Alt-r**
: Obchodzi położenia linijki: u góry widoku, na dole i wyłączona.

**Alt-Shift-e**
: Otwiera listę wcześniej oglądanych plików i pokazuje wybrany.

**Alt-e**
: Zmienia zestaw znaków wyświetlanego tekstu. Przekodowanie idzie z wybranej
strony kodowej na systemową. Aby je wyłączyć, wybierz "\<No translation>" w
oknie wyboru zestawu znaków.

Podglądowi można wskazać, jak ma pokazywać plik, zobacz rozdział
[Plik rozszerzeń](mcommander.md#edit-extension-file).


# Opcje przeglądarki <a id="viewer-options"></a>

Ustawienia
[wbudowanej przeglądarki](#internal-file-viewer),
obowiązujące dla każdego otwieranego pliku.

*Zawijanie długich wierszy.*
Gdy włączone, wiersz szerszy od ekranu jest kontynuowany w następnym wierszu
ekranu; w przeciwnym razie jest obcinany, a widok przewija się w bok. Domyślnie
włączone.

*Podświetlanie składni.*
Gdy włączone, przeglądarka koloruje tekst regułami składni edytora. Plik
otwarty w trybie, który przynosi własne kolory, na przykład strona podręcznika
albo złożony Markdown, zachowuje te kolory. Domyślnie wyłączone.

*Przewijanie stronami myszą.*
Ile przewija kliknięcie w górnej lub dolnej jednej trzeciej widoku: pół ekranu,
gdy włączone, jeden wiersz, gdy nie. Kółka to nie dotyczy, zawsze przewija o
dwa wiersze. Domyślnie włączone.

*Zapamiętywanie pozycji w pliku.*
Gdy włączone, plik otwiera się tam, gdzie go ostatnio zostawiono. Domyślnie
wyłączone.

*Widok drzewa dla JSON, YAML i XML.*
Gdy włączone, plik jednego z tych formatów otwiera się od razu jako zwijane
drzewo, a nie jako zwykły tekst. Ten sam widok jest zawsze dostępny klawiszem
zmiany trybu. Domyślnie wyłączone.

*Znacznik końca pliku.*
Tekst wypisywany po ostatnim wierszu pliku. Domyślnie pusty.

*Najwyżej tyle pominiętych odświeżeń.*
Dopóki plik jest czytany, przeglądarka pomija odświeżenia, żeby nadążyć za
danymi. Tu podaje się, ile może pominąć z rzędu. Domyślnie 10.

*Ograniczenie rozmiaru pliku dla drzewa, MB.*
Największy plik, który widok drzewa analizuje. Większy jest odrzucany przed
odczytem. Domyślnie 64.

*Ograniczenie liczby węzłów drzewa.*
Największe drzewo, które ten tryb buduje, liczone w węzłach. Domyślnie
10000000.

# ZOBACZ TAKŻE <a id="see-also"></a>

mcommander(1), mcedit6(1).
