---
date: wrzesień 2026
---

# NAZWA <!-- help:skip -->

mcommander - wizualny interpetator poleceń dla systemów Unixopodobnych

# UŻYTKOWANIE <!-- help:skip -->

**mcommander**
[-abcCdfPstuUVx] [-l log] [kat1 [kat2]] [-v plik]

# OPIS <a id="description"></a>

M-Commander jest przeszukiwarką katalogów/menedżerem plików dla systemów
Unixopodobnych


# OPCJE <a id="options"></a>

*-a*
: Wyłącza używanie symboli graficznych przy rysowaniu ramek.

*-b*
: Wymusza wyświetlanie czarno-białe.

*-c*
: Wymusza wyświetlanie w kolorze, zobacz sekcję
**Kolory**
żeby zasięgnąć szerszej informacji.

*-d*
: Wyłącza używanie myszy.

*-f*
: Wyświetla wkompilowane ścieżki, w których M-Commander szuka swoich
plików.

*-k*
: Resetuje "miękkie" klawisze do ich standardowych funkcji z termcap/terminfo.
Użyteczne tylko przy terminalach HP, kiedy klawisze funkcyjne nie działają.

*-l plik*
: Zachowuje logi z serwerów ftp do pliku
*plik.*

*-P*
: Przy zakończeniu programu, M-Commander wydrukuje na ekranie katalog,
w którym pracowaliśmy na końcu; to w połaczeniu z funkcją napisaną poniżej
pozwoli ci na przeglądanie swoich katalogów i automatyczne przejście do
tego,
w którym byłeś ostatnio (dziękuję Torbenowi Fjerdingstadowi i Sergeyowi za wkład
w tę funkcję oraz za kod źródłowy, który wprowadzili w życie).

<!-- -->

```
użytkownicy basha i zsh:

mcommander ()
{
        MC=$HOME/tmp/mc$$-"$RANDOM"
        {{bindir}}/mcommander -P "$@" > "$MC"
        cd "`cat $MC`"
        rm "$MC"
        unset MC;
}

użytkownicy tcsh:
alias mcommander 'setenv MC `{{bindir}}/mcommander -P !*`; cd $MC; unsetenv MC'
```

Wiem, że ta funkcja mogłaby być krótsza dla basha i zsh, ale małe cudzysłowy
nie zaakceptowały by zawieszenia programu kombinacją
**C-z**.

*-s*
: Włącza tryb powolnego terminala, w którym program nie będzie rysował zbyt
obciążających znaków graficznych oraz wyłączy opcję weryfikacji.

*-t*
: Używane tylko jeśli kod był skompilowany przy użyciu S-Langa i terminfo:
powoduje, że M-Commander będzie używać zmiennej środowiskowej
**TERMCAP**
do pokazywania informacji terminala, zamiast informacji w systemowej bazie
typów terminali.

*-u*
: Wyłącza używanie równoległej powłoki (ma sens tylko jeśli
M-Commander był kompilowany z obsługą równoległych powłok).

*-U*
: Włącza użycie jednoczesnego inerpretatora poleceń (ma sens tylko jeśli
M-Commander był zbudowany z ustawieniem powłoki w tle jako opcji dodatkowej).

*-v plik*
: Włącza wbudowany podgląd w celu obejrzenia wybranego pliku
*plik.*

*-V*
: Wyświetla wersję programu.

*-x*
: Wymusza włączenie trybu xterm. Używane kiedy działa się na terminalach wyposażonych
w opcje xterm (dwa tryby ekranu i możliwość wysyłania myszą sygnałów wyjścia).

*-X, --no-x11*
: Do not use X11 to get the state of modifiers Alt, Ctrl, Shift

*-g, --oldmouse*
: Force a "normal tracking" mouse mode. Used when running on
xterm-capable terminals (tmux/screen).

Jeśli wybrano, pierwszy katalog używany jest do wyświetlenia w pierwszym panelu.
Drugi wyświetlany jest w drugim panelu.

# Opis <a id="overview"></a>

Ekran M-Commandera podzielony jest na cztery części. Prawie cały obszar
ekranu zajmują dwa panele. Standardowo przedostatnia od dołu linijka ekranu,
przeznaczona jest do wpisywania poleceń, a ostatnia pokazuje klawisze funkcyjne.
Najwyższy wiersz jest wierszem menu. Może on być niewidoczny, ale pojawia się zawsze
po kliknięciu w najwyższą linię ekranu, albo po wciśnięciu klawisza F9.

M-Commander pozwala na oglądanie dwóch paneli w tym samym czasie.
Jeden z nich jest panelem aktywnym (podświetlona linia wyboru znajduje się właśnie
w nim). Niemal wszystkie operacje wykonuje się na panelu aktywnym.
Niektóre operacje, jak
np. kopiowanie, zmiana nazwy używają jako domyślnego miejsca docelowego
katalogu otwartego w panelu nieaktywnym
(nie martw się, zawsze zostaniesz poproszony o
potwierdzenie takiej operacji). W celu zasięgnięcia szerszych informacji zajrzyj
do działów
**Panele katalogów**, **Lewe i prawe menu** oraz **Menu plików**.

Możesz wywoływać dowolne komendy systemowe po prostu wpisując je. Wszystko co
piszesz pojawia się w linii poleceń i po naciśnięciu klawisza Enter zostanie
wykonane przez M-Commandera. Przeczytaj sekcję
**Linia powłoki i Linia wejściowa klawiszy**,
żeby nauczyć się więcej na ten temat.

# Obsługa myszy <a id="mouse-support"></a>

M-Commander obsługuje mysz. Moduł ten jest uruchamiany wtedy kiedy
korzystasz z terminala
**xterm**(1)
(działa nawet wtedy, kiedy łączysz się przez telnet albo rlogin z innym komputerem
z terminala xterm) lub jeśli korzystasz z linuksa na konsoli z zainstalowanym
serwerem
**gpm**(1).

Kiedy klikniesz lewym przyciskiem na panel z katalogami, plik zostanie
wybrany jako aktywny; jeśli klikniesz prawym przyciskiem zostanie on
zaznaczony [lub odznaczony - w zależności od jego aktualnego stanu -
działanie podobne do klawisza
**Insert**
\- przyp. tłumacza].

Podwójne kliknięcie w plik spowoduje wykonanie pliku, jeśli jest on wykonywalny,
a jeśli rozszerzenie pliku jest rozpoznawane przez M-Commander'a i dostępny
jest odpowiedni program, jest on uruchamiany.

Możliwe jest również wykonywanie komend przypisanych klawiszom funkcyjnym
przez kliknięcie w nie.

Jeśli kliknięcie odbędzie się w rejonie górnej lini panelu z katalogami, zostanie
on przewinięty jedną stronę wstecz. Podobnie kliknięcie na dolną ramkę przewija
tekst jedną stronę do przodu. Ta opcja klikania w ramki działa również przy
przeglądaniu pomocy i przy drzewie katalogów.

Standardowo czas autopowtórzenia przy klikaniu myszą wynosi 400 milisekund.
Tę wartość można zmienić edytując plik
~/.config/mc6/ini
i zmieniając parametr
*mouse_repeat_rate.*

Jeśli używasz M-Commandera z obsługą myszy, możesz "przeszczepiać"
kawałki tekstów i używać standardowych zastosowań myszki (kopiowanie i
wklejanie) za pomocą klawisza Shift.

<!-- help:break -->

# Klawisze <a id="keys"></a>

Niektóre komendy M-Commandera wywołuje się kombinacją klawiszy
*Control*
(czasem opisywanego jako CTRL lub CTL) lub
*Meta*
(opisywanego ALT lub nawet Compose). W tym manualu (pliku pomocy) będziemy
używać następujących kombinacji:
C-\<klawisz> - znaczy: trzymając klawisz Control naciśnij
\<klawisz>. Więc C-f będzie oznaczać: trzymając Control, naciśnij f.

M-\<klawisz> - znaczy, że trzymając klawisz Meta lub alt naciskamy \<klawisz>.
Jeśli na twojej klawiaturze nie ma ani klawisza Alt ani Meta, naciśnij ESC, puść
go i wtedy naciśnij \<klawisz> [skutek ten sam, acz jednak użycie trochę
mniej przyjemne i bardziej skomlikowane - przyp. tłumacza].

Wszystkie linie wprowadzające M-Commandera używają w przybliżeniu tych
samych przypisań klawiszy co wersja GNU edytora Emacs.

Jest wiele sekcji mówiących o klawiszach. Ta następująca jest najważniejsza.

Sekcja
[Menu plików](#file-menu)
opisuje skróty klawiszowe do komend pojawiających się w menu plików. Ta sekcja
zawiera funkcję klawiszy. Większość z tych komend wywołuje jakąś akcję przede
wszystkim na jednym lub kilku wybranych plikach.

Sekcja
[Panele katalogowe](#directory-panels)
opisuje klawisze, które zaznaczają plik lub pliki jako docelowe do dalszych
działań (akcją jest najczęściej jedna z tych przedstawionych w menu plików).

Sekcja
*Komendy linii poleceń*
wypisuje listę klawiszy, które są używane do wprowadzania lub edytowania
tekstów w wierszu poleceń. Większość z nich kopiuje nazwy, i inne tego typu,
z panelu katalogów do linii poleceń (żeby uniknąć ich przepisywania), lub
pozwala zwiedzić historię komend linii poleceń.

*Klawisze linii wejściowych*
są używane do edytowania linii na wejściu (przy wpisywaniu). Oznacza,
to że stosuje się je zarówno
do linii poleceń jak do okien dialogowych.

## Klawisze różne <a id="miscellaneous-keys"></a>

Jest tu kilka klawiszy, które nie kwalifikują się do żadnej z wymienionych
powyżej grup:

**Enter**.
Jeśli jest wpisany jakiś tekst w linii poleceń (na samym dole, pod panelami),
to wpisana komenda jest wykonywana. Jeśli nic nie jest wpisane, i linia wyboru
jest na jakimś katalogu, M-Commander wykonuje komendę
**chdir**(2)
(zmiana katalogu) do wybranego katalogu i odświeża zawartość panelu; jeśli
linia wyboru jest na pliku wykonywalnym jest on wykonywany. I wreszcie jeśli
rozszerzenie pliku zgadza się z obługiwanym przez programy zewnętrzne, które
są obsługiwane prze M-Commandera, są one wywoływane z owym programem.

**C-l**.
Od nowa rysuje wszystkie informacje okna M-Commandera.

**C-x c**.
Uruchamia komendę Chmod dla aktualnego pliku lub zaznaczonych plików.

**C-x o**.
Uruchamia komendę Chown dla aktualnego pliku lub zaznaczonych plików.

**C-x l**.
Uruchamia komendę dowiązywania.

**C-x s**.
Uruchamia komendę miękkiego dowiązywania.

**C-x i**.
Zmienia aktywny panel.

**C-x q**.
Przełacza nieaktywny panel w tryb "quick view".

**C-x !**.
Wykonuje komendę z zewnętrznego panelu.

**C-x h**.
Uruchamia komendę dodawania katalogów do hotlisty.

**M-!**.
Uruchamia komendę filtrowanego podglądu, opisanego w sekcji
*Podgląd.*

**M-?**.
Uruchamia komendę szukania pliku.

**M-c**.
Włącza okno dialogowe quick cd (szybkiej zmiany katalogów)

**C-o**.
Jeśli program jest uruchamiany na konsoli typu Linux lub FreeBSD lub też
na konsoli xterm, pokaże wyjście ostatnio wykonywanego programu. Jeśli
uruchomiono M-Commandera na konsoli type Linux, M-Commander używa
zewnętrznego programu (cons.saver) w celu zachowywyania i odzyskiwania
informacji na ekranie komputera.

Jeśli użycie trybu powłoki w tle jest wkompilowane, możesz nacisnąć
C-o w dowolnej chwili i zostataniesz przeniesiony z powrotem bezpośrednio
do głównego okna M-Commandera, żeby powrócić do wykonywania aplikacji
po prostu naciśnij znów C-o. Jeśli masz zawieszoną aplikację właśnie przez
użycie tego triku, nie będziesz mógł "odpalać" innych programów spod
M-Commandera dopóki nie zamkniesz zawieszonego programu.

Aby dowiedzieć się czegoś na temat polskiech liter w M-Commanderze
przeczytaj sekcję
*Polskie litery.*

## Panel Katalogów <a id="directory-panels"></a>

Sekcja opisuje klawisze, które operują na panelu katalogów. Jeśli chcesz
wiedzieć jak zmienić panele zobacz sekcję
*Lewe i prawe menu.*

**Tab**, **C-i**.
Zmienia aktywny panel. Stary panel staje się w tym momencie aktywnym panelem,
a aktywny staje się starym. Linia wyboru zmienia swoje położenia do aktywnego
panelu.

**Insert**, **C-t**.
DEPRECATED! Do zaznaczania plików możesz używać klawisza Insert lub C-t. Żeby odznaczyć plik
po prostu zaznacz jakiś już zaznaczony.

**Insert**
: to tag files you may use the Insert key (the kich1 terminfo sequence).
To untag files, just retag a tagged file.

**M-e**
: to change charset of panel you may use M-e (Alt-e).
Recoding is made from selected codepage into system codepage. To
cancel the recoding you may select "directory up" (..) in active panel.
To cancel the charsets in all directories, select "No translation " in
the dialog of encodings.

**M-g**, **M-r**, **M-j**.
Używane do wybierania najwyższego, środkowego i najniższego pliku w panelu.

**M-t**.
Przełącza tryb wyświetlania do następnego możliwego. Używając tej opcji
łatwo jest przejść szybko z długiego do krótkiego trybu wyświetlania
jak również do tego zdefiniowanego przez użytkownika.

`C-\`
(control-backslash).
Pokazuje hotlistę katalogów i zmienia katalog do wybranego przez użytkownika.

**+**
(plus).
Używane do zaznaczania grupy plików. M-Commander zapyta o
wyrażenie opisującą grupę. Jeśli opcja
*Shell Patterns*
jest włączona, typ wyrażeń jest bardzo podobny do tego w powłoce
(\* dla zera i więcej znaków i ? dla jednego znaku). Jeśli zaś opcja
*Shell Patterns*
jest wyłączona, sposób zaznaczania plików jest zgodny z ustawieniami
(zobacz
**ed**(1)).

`\`
(backslash).
Używaj znaków "\\" do odznaczania grupy plików. Jest to przeciwieństwo klawisza
plus.

**strzałka do góry**, **C-p**.
Przenosi linię wyboru do poprzedniej pozycji w panelu.

**strzałka do dołu**, **C-n**.
Przenosi linię wyboru do następnej pozycji w panelu.

**home**, **a1**, **M-<**.
Przenosi linię wyboru do pierwszej pozycji w panelu.

**end**, **c1**, **M->**.
Przenosi linię wyboru do ostatniej pozycji w panelu.

**PageDown**, **C-v**.
Przenosi linię wyboru jedną stronę do dołu.

**PageUp**, **M-v**.
Przenosi linię wyboru jedną stronę do góry.

**M-o**.
Jeśli drugi panel jest zwykłym panelem wyświetlającym i w aktywnym panelu
stoisz na katalogu, drugi panel będzie
pokazywać zawartość
akutalnego katalogu (tak jak w Emacsie kombinacja C-o). Jeśli nie stoisz
na katalogu zawartością drugiego katalogu stanie się katalog o jedno piętro
wyższy od aktualnego.

**C-PageUp**, **C-PageDown**.
Działa tylko na konsoli typu Linux: wykonuje przejście do katalogu ".." lub
do aktualnie wybranego, w zależności od kombinacji.

**M-y**.
Przenosi do poprzedniego katalogu w historii, podobne do kliknięcia myszką.
'<'.

**M-u**.
Przechodzi do następnego katalogu w historii, podobne do kliknięcie myszką
w '>'.

**M-S-h**, **M-H**.
Wyświetla historię katalogów, podobne działanie do kliknięcia myszką 'v'.

## Quick search

**C-s**, **M-s**.
Uruchamia szukanie pliku w katalogu na podstawie jego nazwy. Kiedy szukanie
jest aktywne, każde naciśnięcie klawisza doda jeden znak do poszukiwania
zamiast wypisania go linii poleceń. Jeśli opcja
*Show mini-status*
jest włączona, szukany ciąg znaków pojawia się w linii mini-statusu. Kiedy
wpisujemy znak, linia wyboru przemieszcza się do następnego pliku zaczynającego
się od podanych liter. Klawisze
*backspace*
lub
*del*
mogą być używane do poprawiania błędów. Jeśli C-s zostanie naciśnięte ponownie,
M-Commander rozpoczyna szukanie następnego pliku
zaczynającego się od podanych znaków.

## Linia Powłoki <a id="shell-command-line"></a>

Ta sekcja opisuje klawisze, które są użyteczne do efektywniejszego
wpisywania podczas podawania komend powłoki.

**M-Enter**.
Kopiuje nazwę aktualniego wybranego pliku do linii poleceń.

**C-Enter**.
To samo co M-Enter, działa tylko na konsoli typu Linux.

**M-Tab**.
Wykonuje dokończenie nazw plików, komend, zmiennych, użytkowników, nazw hostów
za Ciebie.

**C-x t**, **C-x C-t**.
Kopiuje nazwy zaznaczonych plików (lub jeśli nie ma zaznaczonych - aktywnego)
w aktywnym (C-x t) lub nieaktywnym panelu (C-x C-t) do linii poleceń.

**C-x p**, **C-x C-p**.
Pierwsza kombinacja kopiuje pełną ścieżkę z aktywnego, a druga z nieaktywnego
panelu.

**C-q**.
Komenda 'quote' (cytuj) może być używana do wpisywania do wiersza poleceń znaków, które
normalnie przechwytywane są przez Commandera (tak jak znak '+').

**M-p**, **M-n**.
Używaj tych klawiszy, żeby przeglądać historię komend. M-p wyświetla poprzednią,
a M-n następną komendę.

**M-h**.
Wyświetla historię aktualnej linii poleceń.

## Podstawowe klawisze ruchu <a id="general-movement-keys"></a>

Przeglądarka pomocy, podgląd plików i drzewo katalogów używają podobnych
klawiszy do przemieszczania. Przez to akceptują dokładnie te same klawisze.
Każde z nich z resztą traktują je jako swoje własne.

Niektóre partie M-Commandera również używają tych klawiszy,
więc niniejsza sekcja może być użyteczna również dla tych partii.

**strzałka w górę**, **C-p**.
Przechodzi jedną linię wstecz.

**strzałka w dół**, **C-n**.
Przechodzi jedną linię naprzód.

**Page Up**, **M-v**.
Przechodzi jedną stronę wstecz.

**Next Page**, **Page Down**, **C-v**.
Przechodzi jedną stronę naprzód.

**Home**, **A1**.
Przechodzi do początku.

**End**, **C1**.
Przechodzi na koniec.

Przeglądarka pomocy i podgląd plików akceptują następujące klawisze
(poza tymi opisanymi powyżej).

**b**, **C-b**, **C-h**, **Backspace**, **Delete**.
Przechodzi jedną stronę wstecz.

**klawisz spacji**.
Przechodzi jedną stronę naprzód.

**u**, **d**.
Przechodzi pół strony naprzód lub wstecz.

**g**, **G**.
Przechodzi do początku lub do końca.

## Linia wejściowa klawiszy <a id="input-line-keys"></a>

Linie wejściowe (te używane w linii komend i w oknach dialogowych), akceptują
następujące klawisze:

**C-a**.
umieszcza kursor na początku linii.

**C-e**.
umieszcza kursor na końcu linii.

**C-b**, **move-left**.
przenosi kursor o jedną pozycję w lewo.

**C-f**, **move-right**.
przenosi kursor o jedną pozycję w prawo.

**M-f**.
przesuwa kursor o jedno słowo naprzód.

**M-b**.
przesuwa kursor o jedno słowo wstecz.

**C-h**, **backspace**.
kasuje poprzedni znak.

**C-d**, **Delete**.
kasuje znak w miejscu kursora (nad nim).

**C-@**.
wstawia zaznaczenie do kasowanie (patrz następne pozycje).

**C-w**.
kopiuje tekst spomiędzy kursora i zaznaczenia do bufora i usuwa go z linii
poleceń.

**M-w**.
to samo co C-w tylko, że nie usuwa tekstu z linii.

**C-y**.
wstawia spowrotem zawartość wyciętego bufora.

**C-k**.
wycina tekst od kursora do końca linii.

**M-p**, **M-n**.
Używaj tych klawiszy, żeby przeglądać historię komend. M-p wyświetla poprzednią,
a M-n następną.

**M-C-h**, **M-Backspace**.
kasuje jedno słowo wstecz (poprzednie).

**M-Tab**.
Wykonuje dokończenie nazw plików, komend, zmiennych, użytkowników, nazw hostów
za Ciebie.

<!-- help:break -->

# Linia menu <a id="menu-bar"></a>

Linia menu uaktywnia się kiedy wciskasz klawisz F9 lub kiedy klikasz myszką
na najwyższy wiersz ekranu. Linia menu ma pięć podmenu: "left", "file", command",
"options" i "right" (po polsku to jest "lewe", "plik", "komendy", "opcje",
"prawe").

Lewe i prawe menu pozwalają ci na modyfikacje wyglądu lewego i prawego panelu
katalogowego.

Menu plik pozwala na wykonanie akcji na aktualnym lub zaznaczonych plikach.

Menu komend mieści w sobie możliwe do wykonania akcje, które są dużo bardziej
globalne i nie mają związku z aktualnym i zaznaczonymi plikami.

## Lewe i prawe menu <a id="left-and-right-menus"></a>

Wygląd panelu katalogowego może zostać zmieniony poprzez menu
**left**
i
**right**.

### Tryby wyświetlania (Listing modes) <a id="listing-format"></a>

Tryby wyświetlania są używane do zmienia ustawień przy wyświetlaniu.
Dostępne są cztery różne tryby:
**Full**,
**Brief**,
**Long**
i
**User**.
Tryb "Full" pokazuje nazwę, rozmiar i czas modyfikacji pliku.

Tryb "Brief" pokazuje tylko nazwę pliku i ma dwie kolumny (dzięki temu
może pokazywać nawet dwa razy więcej niż inne tryby). Tryb "Long" jest
podobny do wyniku polecenia
**ls -l**.
Zabiera on szerokość całego ekranu.

Jeśli wybierzesz tryb "user" (użytkownika), będziesz mógł wybrać własny
sposób wyświetlania.

Tryb użytkownika musi zaczynać się od określenia wielkości panelu. Może
to być "half" (pół) lub "full" (cały) i określa, czy ma być widoczny
jeden duży panel na cały ekran czy dwa mniejsze.

Po rozmiarze panelu możesz włączyć tryb dwóch kolumn panelu. Robi się
to dodając liczbę "2" do tekstu formatu.

Po tym wpisujesz już nazwy pól z podaniem opcjonalnej wielkości.
Wszystkie możliwe pola jakich możesz użyć to:

**name**
: wyświetla nazwę pliku.

**size**
: wyświetla wielkość pliku.

**bsize**
: jest alternatywą dla format
**size**.
Wyświetla rozmiar plików, a dla katalogów po prostu wyświetla tekst
"SUB-DIR" lub "UP--DIR".

**type**
: wyświetla jednoznakowy opis typu pliku. Ten znak jest taki sam co ten
wyświetlany prze komendę ls z flagą -F. Wyświetlana jest gwiazdka
dla plików wykonywalnych,
ukośnik dla katalogów, małpa (@) dla dowiązań, znak równości dla gniazd,
minus dla urządzeń niestniejących, znak plus dla urządzeń istniejących,
pionową kreskę (|) dla kolejek FIFO, tyldę dla dowiązań
symbolicznych, i wykrzyknik dla dowiązań wskazujących na nieistniejący plik.

**mark**
: Gwiazdka jeśli plik jest zaznaczony, spacja jeśli nie jest.

**mtime**
: czas ostatniej modyfikacji pliku.

**atime**
: czas ostatniego dostępu do pliku.

**ctime**
: czas utworzenia pliku.

**perm**
: tekst reprezentujący aktualne uprawnienia do pliku.

**mode**
: wartość (cyfrowa) przedstawiająca prawa do pliku.

**nlink**
: liczba dowiązań do pliku.
**ngid**
GID (numeryczny).

**nuid**
: UID (numeryczny).

**owner**
: właściciel pliku.

**group**
: grupa pliku.

**inode**
: numer i-węzła pliku.

Możesz również używać następujących znaków dla zmiany wyświetlania:

**space**
: spacja w formacie wyświetlania.

**|**
: Ten znak jest używany w celu dodania pionowej linii od formatu wyświetlania.

Żeby wymusić szerokość pola, po prostu dodaj ':' a potem ilość znaków jakie
chcesz żeby miało pole. Jeśli numer zaczyna się od '+', to szerokość nie może
być mniejsza od podanej, jeśli program zobaczy, że jest jeszcze trochę
miejsca na ekranie, rozszerzy to pole.

Na przykład tryb
**Full**
wyświetla w formacie:

half type name | size | mtime

A format
**Long**
wyświetla w formacie:

full perm space nlink space owner space group space size space mtime
space name

A to jest całkiem ładny tryb użytkownika:

half name | size:7 | type mode:3

Panele mogą być również przestawione do następujących trybów:

**Info**
: Tryb info wyświetla informację o aktualnie zaznaczonym pliku i (jeśli
to możliwe) o systemie plików.

**Tree (drzewo)**
: Widok drzewa jest całkiem podobny do widoku
[Drzewa katalogów](#directory-tree).
Zobacz tę sekcję jeśli chcesz się dowiedzieć czegoś na ten temat.

**Quick View**
: W tym trybie, panele zostaną przełączone w tryb zredukowanego podglądu
wyświetlającego zawartość aktualnego pliku. Jeśli zaznaczysz panel
(klawiszem tab lub myszką), będziesz miał dostęp do większości komend
podglądu.

### Porządek sortowania (Sort order...) <a id="sort-order"></a>

Istnieje osiem porządków sortowania. Przez: nazwę, rozszerzenie,
datę modyfikacje, datę odczytu, datę zmiany, rozmiar,
numeru i-węzła i niesortowane. Porządek sortowanie możesz wybrać w oknie
dialogowym porządku sortowania. Możliwe jest również wybranie porządku
wstecznego (od tyłu).

Standardowo, katalogi są sortowane przed plikami, ale może to być zmienione
przez opcję
**Mix all files (mieszaj wszystkie pliki)**.

### Filtry (Filter...) <a id="filter"></a>

Komenda filtra pozwala ci na podanie rozszerzenia, które musi być spełnione,
żeby pliki były widoczne (na przykład
**\*.tar.gz**).
Niezależnie od filtru katalalogi i dowiązania do katalogów są zawsze pokazywane.

### Odśwież (Reread) <a id="reread"></a>

Komenda odśwież odświeża widok wszystkich plików w katalogów. Jest to użyteczne
jeśli inny proces stworzył lub usunął jakiś pliki. Jeśli użyłeś panelu
zewnętrznego, wszystkie informacje zostaną przywrócone do prawdziwego stanu.

## Menu plików (File menu) <a id="file-menu"></a>

M-Commander używa klawiszy F1 - F10 jako skrótów klawiszowych do komend
występujących w menu plików. Na terminalach bez funkcji klawiszowych (F1 - F10)
można używać kombinacji klawisza Escape i numeru ( odpowiednio 1 dla F1,
2 dla F2 itd. )

Menu plików ma następujące komendy (skróty klawiszowe umieszczone są
na dole ekranu):

**Pomoc (F1)**

Wywołuje wbudowaną przeglądarkę plików pomocy. Wewnątrz niej można używać
klawisza Tab żeby przejść do następnego dowiązania, Enter
żeby przejść do wybranego dowiązania. Klawisze Spacji i Backspace są używane
do poruszania się naprzód i wstecz na stronach pomocy. Naciśnij klawisz
F1 żeby uzyskać pełną listę dostępnych klawiszy w pomocy.

**Menu (F2)**

Wywołuje menu użytkownika. Menu użytkownika jest łatwym w użyciu narzędziem
służącym do obsługi zewnętrznych programów i dodatkowych opcji
M-Commandera.

**Podgląd (F3, Shift-F3)**

Włącza podgląd aktualnie wybranego pliku. Standardowowo wywoływany jest
wbudowany podgląd plików, ale jeśli opcja "Use internal view" jest wyłączona,
wywoływany jest zewnętrzny program do poglądu, wskazywany przez zmienną
**PAGER**.
Jeśli jednak zmienna
**PAGER**
nie została jeszcze zdefiniowana, wywoływana jest komenda "view". Jeśli użyjesz
kombinacji klawiszy
**Shift-F3**,
pogląd zostanie wywołany bez jakiegokolwiek
formatownia pliku.

**Filtrowany podgląd (M-!)**

Ta kombinacja klawiszy oczekuje na komendę i jej argument (argumentem standardowo
jest wybrany aktualnie plik), całe wyjście programu przekierowywane jest do pliku,
który zostaje automatycznie wyświetlony na ekranie w trybie podglądu.

**Edycja (F4)**

Aktualnie ta komenda wywołuje edytor
**vi**(1)
lub edytor wybrany w zmiennej środowiskowej, lub wbudowany wewnętrzny edytor
plików jeśli opcja use_internal_edit jest włączona.

**Kopiuj (F5)**

Włącza okno dialogowe, w którym standardowo znajduje się ścieżka do
katalogu w
nieaktywnym panelu, po czym kopiuje aktualny plik (lub wybrane
jeśli wybrano jakiekolwiek) do katalogu, który wybraliśmy w oknie dialogowym.
Space for destination file may be preallocated relative to preallocate_space
configure option.
Podczas procesu kopiowania możesz go w każdej chwili przerwać wciskając C-c lub
Esc. Żeby dowiedzieć się czegoś więcej na temat jokerów w ścieżce źródłowej
(którymi najczęściej będą \* lub ^\\(.\*\\)$) i innych możliwych określeń w
katalogu docelowym zobacz kategorię
**Maski kopiowania/przenoszenia**

Na niektórych systemach możliwe jest kopiowanie w tle, robi się to klikając
na przycisk backgorund (lub naciskając kombinację M-b w oknie dialogowym).
Background Jobs jest używane do kontrolowania prac w tle.

**Link (C-x l)**

Tworzy sztywne dowiązanie do aktualnego pliku.

**SymLink (C-x s)**

Tworzy symboliczne dowiązanie do aktualnego pliku. Dla tych, którzy nie wiedzą
co to jest dowiązanie: tworzenie dowiązania do pliku jest tak jak kopiowanie
pliku, z tym tylko,
że zarówno plik źródłowy i docelowy reprezentują ten sam plik. Na przykład,
jeśli edytujesz jeden z tych plików, zmiany, które czynisz pojawiają się w obu
plikach. Niektórzy mówią na dowiązania aliasy lub skróty.

Twarde dowiązanie wydaje się być prawdziwym plikiem. Po stworzeniu go
nie ma możliwości
rozróżnienia, który z plików jest oryginalny, a który jest dowiązaniem.
Jest bardzo
ciężko zauważyć, że wskazują one na ten sam plik.
Używaj dowiązań twardych wtedy kiedy nie chcesz tego wiedzieć.

Dowiązanie symboliczne jest tylko odwołaniem do oryginalnego pliku.
Jeśli ten plik
zostanie wyrzucony, dowiązanie stanie się bezużyteczne. Jest całkiem łatwo
zauważyć,
że pliki odnoszą się w gruncie rzeczy do tego samego. M-Commander
pokazuje znak "@" przed nazwą pliku jeśli jest dowiązaniem
symbolicznym do innych
(poza katalogami, przed którymi pokazuje tyldę (~)). Oryginalny plik wskazywany
przez dowiązanie jest pokazywany w linii mini-statusu, jeśli opcja
*Show mini-status*
jest włączona. Używaj dowiązań symbolicznych, jeśli chcesz unikąć problemów z
rozpoznawaniem twardych dowiązań.

**Zmiana nazwy/przeniesienie (F6)**

Włącza okno dialogowe, gdzie standardowo wpisana jest nazwa katalogu w
nieaktywnym panelu, i przenosi aktualnie wybrany plik (lub zaznaczone jeśli
choć jeden jest zaznaczony) do katalogu wpisanego w oknie dialogowym. Podczas
procesu przenoszenia możesz użyć kombinacji klawiszy C-c lub ESC, żeby przerwać
operację. Po więcej szczegółów zobacz operację
**Kopiuj**
opisaną powyżej. Większość rzeczy jest całkiem podobna.

Na niektórych systemach możliwe jest przenoszenie w tle, robi się to klikając
na przycisk background (lub naciskając kombinację M-b w oknie dialogowym).
Background Jobs jest używane do kontrolowania prac w tle.

**Utwórz katalog (F7)**

Włącza menu dialogowe i zakłada katalog o podanej nazwie

**Kasuj (F8)**

Kasuje aktualnie wybrany lub zaznaczone pliki w aktywnym panelu. Podczas
procesu możesz nacisnąć C-C lub Esc żeby przerwać operację. [skasowane pliki
nie będą jednak odzyskane - przyp. tłumacza].

**Zaznacz grupę (+)**

Używane do zaznaczania grupy plików. M-Commander będzie żądał tekstu
opisującego grupę plików. Jeśli opcja
*Shell Patterns*
jest włączona, tekst będzie traktowany jako globalny dla interpretatora (\*
oznacza zero lub więcej znaków a ? oznacza jeden znak). Jeśli opcja
*Shell Patterns*
jest wyłączona, wtedy zaznaczanie plików jest robione z zastosowaniem norm
zewnętrznych (zobacz ed (1)).

**Odznacz grupę (\\)**

Używane do odznaczania grupy plików. Jest przeciwieństwem komendy
*Zaznacz pliki.*

**Wyjdź (F10, Shift-F10)**

Zamyka M-Commandera. Shift-F10 jest używany jeśli używasz
"wrappera" powłoki. Shift-F10 nie przeniesie cię do katalogu, w którym
byłeś ostatnio w M-Commanderze, zamiast tego przejdzie do katalogu,
z którego uruchomiłeś program.

### Szybka zmiana katalogów (Quick cd) M-c <a id="quick-cd"></a>

Ta komenda jest bardzo użyteczna, jeśli masz już pełną linię poleceń, a
chcesz przejść do innego katalogu. Uruchamia ona małe okno dialogowe,
w którym podajesz to co po normalnej komendzie
**cd**
po czym naciskasz Enter. Wszystkie opcje są dokładnie takie same jak we
wbudowanej komendzie cd.

## Menu komend (Command Menu) <a id="command-menu"></a>

Komenda drzewo katalogów (Directory tree) pokazuje drzewo katalogów.

Komenda "Find file" szuka pliku spełniającego podane warunki, natomiast komenda
"Swap panels" zamienia zawartości obu paneli.

Komenda "Panels on/off" pokazuje wyjście ostatniej komendy interpetatora
poleceń. Działa ona tylko na terminalach typu Linux lub FreeBSD.

Komenda porównywania katalogów (Compare directories) (C-x d) porównuje
zawartości panelu katalogowego z drugim. Możesz potem użyc Kopiuj (F5)
żeby stworzyć dwa dokładnie identyczne panele. Metoda "quick" porównuje
tylko i wyłącznie rozmiary plików i ich daty. Metoda "thorough" porównuje
pliki bajt po bajcie. Metoda "size-only" zwraca uwagę tylko na rozmiar plików.
Nie ma dla niej żadnego znaczenia czy plik ma inną datę lub zawartość, liczy
się tylko rozmiar.

Komenda historii komend (Command history) pokazuje listę wpisanych komend.
Ta, którą wybierzesz, jest kopiowana do linii poleceń. Do historii komend
można mieć dostęp również przy użyciu kombinacji M-p lub M-n.

Komenda hotlisty katalogów (Directory hotlist) (C-\\) pozwala na zmienianie
katalogów do tych najczęściej używanych dużo szybciej.

Komenda panelu zewnętrznego (External panelize) pozwala na wykonywania programów
zewnętrznych i ustawienia zawartości paneli na to co zwróciła wywołana
komenda.

Komenda edycji rozszerzeń plików (Edit Extension File) pozwala na własny wybór
programów, które mają być używane do wykonywania plików z podanymi
rozszerzeniami. Komenda edycji pliku menu (Edit Menu File) może być używana do
edytowania menu użytkownika (tego, które pojawia się po naciśnięciu kombinacji
F2).

### Drzewo katalogów (Directory Tree) <a id="directory-tree"></a>

Możesz wybierać katalogi z drzewa katalogów i M-Commander przejdzie do
wybranego przez Ciebie katalogu.

Są dwa sposoby wywoływania drzewa. Prawdziwa komenda drzewa katalogów jest
dostępna z menu komend. Inną metodą jest wybranie drzewa z menu "lewego" bądź
"prawego".

Żeby nie mieć zbyt dużych opóźnień M-Commander skanuje tylko małą
ilość katalogów (tę potrzebną w danej chwili). Jeśli jakiegoś katalogu nie
widać przejdź do jego katalogu nadrzędnego i naciśnij C-r (lub F2).

Możesz używać następujących klawiszy:

Generalne klawisze ruchu są akceptowane.

**Enter**.
W drzewie katalogów, wychodzi z trybu drzewa i przechodzi znów do trybu
zwykłego panelu. W podglądzie drzewa zmienia katalog w drugim panelu i zostaje
w trybie podglądu drzewa w panelu aktywnym.

**C-r**, **F2** (Rescan).
Odświeża aktualny katalog. Używane jeśli drzewo nie jest już aktualne. Nie
pokazuje katalogów, które już istnieją lub pokazuje te, których już nie ma.

**F3** (Forget).
Usuwa aktualny katalog z drzewa katalogów. Używaj tego jeśli chcesz usunąć
"śmiecące" i niepotrzebne katalogi z wyświetlania. Żeby były one znów
widoczne wystarczy nacisnąć F2.

**F4** (Static/Dynamic).
Przełącza pomiędzy dynamicznym (standardowo) i statycznym trybem nawigacji.

W trybie statycznym możesz używać strzałek do dołu i do góry do wybierania
katalogu. Wszystkie zwiedzone katalogi są widoczne.

W trybie dynamicznym możesz używać strzałek w celu wybrania równorzędnego
katalogu, strzałki w lewo żeby dostać się do katalogu domowego, strzałki
w prawo w celu dostania się do katalogu podrzędnego. Widoczne jest tylko
najbardziej aktualne drzewo katalogów. Drzewo zmienia się więc dynamicznie
podczas twojego przemieszczania.

**F5**
(Copy).
Kopiuje katalog.

**F6**
(RenMov).
Przenosi katalog.

**F7**
(Mkdir).
Tworzy nowy katalog poniżej aktualnego.

**F8**
(Delete).
Kasuje katalog z systemu plików.

**C-s**, **M-s**.
Szuka natępnego katalogu spełniającego podane warunki szukania. Jeśli taki
nie istnieje te klawisze spowodują przemieszczenie się o jedną linię w dół.

**C-h**, **Backspace**.
Kasuje ostatni znak w ciągu znaków do poszukiwania.

**Jakikolwiek inny klawisz**.
Dodaje klawisz do ciągu znaków do szukania i przenosi do najbliższego
katalogu, którego nazwa zaczyna się od tych znaków. W podglądzie drzewa musisz
najpierw uaktywnić szukanie naciskając C-s. Ciąg szukający jest pokazywany
w linii mini-statusu.

Następujące klawisze są dostępne tylko w drzewie katalogów. Nie działają one
w poglądzie katalogów.

**F1**
(Help).
Wywołuje podgląd pomocy i pokazuje tę sekcję.

**Esc**, **F10**.
Wychodzi z drzewa. Nie zmienia katalogów.

Mysz jest obsługiwana. Podwójne kliknięcie ma znaczenie identyczne do
klawisza Enter. Zobacz również sekcję
*Obsługa myszy.*

### Znajdź plik (Find File) <a id="find-file"></a>

Komenda znajdź plik najpierw pyta się o startowy katalog do przeszukiwania
i o nazwę pliku, który ma być znaleziony. Wciskając przycisk "Tree" (drzewo)
możesz wybrać katalog startowy z drzewa katalogów.

Pole trzecie akceptuje wszystkie wyrażenia podobne do tych w egrep(1).
Oznacza to, że musisz rozpoczynać znaki o specjalnym znaczeniu kombinacją
"\\" np. szukając "strcmp (" będziesz musiał wpisać "strcmp \\(" (bez
cudzysłowów oczywiście).

Możesz zacząć przeszukiwanie naciskając przycisk Ok. Podczas szukania możesz
zatrzymać proces przy użyciu przycisku Stop i kontynuować po naciśnięciu
Startu.

Możesz przeglądać liste znalezionych plików za pomocą strzałek do dołu
i do góry. Komenda Chdir przejdzie do katalogu aktualnie wybranego. Przycisk
Again zapyta się o nowe parametry do szukania (rozpocznie proces od nowa).
Przycisk Quit kończy przeszukiwanie. Przycisk Panelize umieści znalezione
pliki w aktywnym panelu katalogowym tak, że będziesz mógł wykonywać na nich
standardowe czynności (podgląd, kopiowanie, przenoszenie, kasowanie itp.).
Po spanelizowaniu wystarczy naciśnąć C-r żeby powrócić do normalnego trybu.

Możliwe jest posiadanie listy katalogów, których szukanie plików nie
powinno uwzględniać (na przykład możesz chcieć ominąć przeszukiwanie CDROMu
i innych podmontowanych systemów plików).

Katalogi do omijania powinny być umieszczone w zmiennej
**ignore_dirs**
w sekcji
**FindFile**
twojego pliku ~/.config/mc6/ini.

Składowe katalogów powinny być oddzielone od siebie przez średniki, to jest
przykład:

```
[FindFile]
ignore_dirs=/cdrom:/nfs/wuarchive:/afs
```

Możesz woleć używać panelu zewnętrznego do wykonywania niektórych operacji.
Szukanie pliku jest dobre tylko dla prostych zapytań. Używając panelu
zewnętrznego możesz dokonywać tak skomplikowanych wyszukiwań jak tylko
pragniesz.

### Panel zewnętrzny <a id="external-panelize"></a>

Panel zewnętrzny pozwala ci na wykonywanie zewnętrznych programów i
oglądanie ich wyjścia jako zawartości aktywnego panelu.

Na przykład, jeśli chcesz aby w aktywnym panelu wyświetlone zostały
wszystkie dowiązania w aktywnym katalogu, możesz użyć panelu zewnętrznego
i następującej komendy:

```
find . -type l -print
```

Zanim komenda zakończy działanie, zawartość katalogów nie będzie już dłużej
zawartością aktualnego katalogu, ale wszystkie pliki będą symbolicznymi
dowiązaniami.

Jeśli chcesz wyświetlić wszystkie pliki, które ściągnąłeś ze swoich
serwerów ftp, możesz użyć tej komendy awk żeby wypisać nazwę pliku z
logów transferu:

```
awk '$9 ~! /incoming/ { print $9 }' < /var/log/xferlog
```

Możesz zapisać sobie często używane komendy pod jakąś nazwą, po to
żeby móc ich potem używać dużo łatwiej. Robisz to po prostu wpisując komendę
w linii wejściowej, a potem naciskająć przycisk Add. Potem wpisujesz nazwę,
pod jaką ta komenda ma być widoczna. Następnym razem po prostu wybierasz
tę komendę z listy i nie musisz już wpisywać jej ponownie.

### Hotlist

Hotlista katalogów pokazuje nazwy katalogów wprowadzonych do hotlisty.
M-Commander zmieni miejsce do tego, które wskazuje nazwa katalogu.
Z hotlisty możesz wyrzucać już dodane pozycje par nazw/wskazań i dodawać nowe.
Dla dodawania możesz wykorzystać kombinację (C-x h), która dodaje
ścieżkę
aktualnego katalogu do hotlisty. Użytkownik musi tylko podać pod jaką
nazwą ma być ten katalog widoczny.

Powoduje to przechodzenie do częściej przeglądanych katalogów znacznie szybciej.
Możesz używać ciągle wartości CDPATH opisanej w sekcji Wewnętrzne
przemieszczanie.

### Edycja rozszerzeń pliów (Edit Extension File) <a id="edit-extension-file"></a>

Ta komenda wywoła twój edytor na plik
*~/.config/mc6/extensions.ini.*
If this file does not exist and you are not root, it will be copied from
*{{sysconfdir}}/mcommander/extensions.ini.*
If you are root, you can choose the file to edit: user's
*~/.config/mc6/extensions.ini*
or system-wide
*{{sysconfdir}}/mcommander/extensions.ini.*
The format of this file is described in detail in it.
PP

### Prace w tle (Background jobs) <a id="background-jobs"></a>

Pozwalają ci one kontrolować status jakichkolwiek procesów wykonywanych
w tle
przez M-Commandera (tylko operacje kopiowania i przenoszenia, mogą
być wykonywane w tle). Z tego menu możesz zastopować, zresetować i "zabić"
proces w tle.

### Edycja menu użytkownika (Edit Menu File) <a id="edit-menu-file"></a>

Menu użytkownika jest bardzo użytecznym menu, które może być tworzone
w sposób dowolny, przez użytkownika. Kiedy tylko próbujesz coś zrobić
przy użyciu tego menu, ładowany jest plik .usermenu z aktualnego katalogu, ale
tylko wtedy kiedy jest on w posiadaniu użytkownika lub roota i mamy do niego
prawa zapisu. Jeśli takiego nie ma próbuje się z plikiem ~/.config/mc6/menu z tymi
samymi założeniami, jeśli jego też nie ma - używa się standardowego pliku
systemowego, który znajduje się w {{pkgdatadir}}/usermenu.

Format pliku z menu użytkownika jest bardzo prosty. Linie zaczynające się
od czegokolwiek innego niż spacja lub tabulacja, są traktowane jako
wtyczki do menu (aby móc używać ich potem jako gorących klawiszy, dobrze
jest aby pierwszy znak był literą). Wszystkie linie zaczynające od spacji
lub tabulacji, są komendami, które mają być wykonane jeśli wtyczka zostanie
wybrana.

Kiedy opcja zostaje wybrana, wszystkie komendy należące do tej opcji
kopiowane są do pliku w katalogu tymczasowym (najczęściej do /usr/tmp), a
potem plik jest wykonywany. Pozwala to użytkownikowi wkładać normalne
konstrukcje powłoki do konstrukcji kodu wykonywanego. Po więcej
informacji zobacz, używania makr.

To jest przykładowy plik usermenu:

```
A	Wyrzuć aktualny plik.
	od -c %f

B	Stwórz raport o błędzie i wyślij do roota.
	I=`mktemp ${MC_TMPDIR:-/tmp}/mail.XXXXXX` || exit 1
	vi $I
	mail -s "Błąd M-Commandera" root < $I
	rm -f $I

M	Przeczytaj pocztę.
	emacs -f rmail

N	Przeczytaj grupę dyskucyjną.
	emacs -f gnus

J	Skopiuj rekursywnie cały aktualny katalog.
	tar cf - . | (cd %D && tar xvpf -)

= f *.tar.gz | f *.tgz & t n
X       Zdekompresuj skompresowany plik tar.
	tar xzvf %f
```

**Standardowe warunki**

Każda opcja może być opatrzona w warunki. Warunek musi zaczynać się od
pierwszej kolumny i od znaku '='. Jeśli warunek jest prawdziwy, opcja
stanie się opcją domyślną.

```
Składnia warunku: 	= <warunek>
	    lub:	= <warunek> | <warunek> ...
	    lub:	= <warunek> & <warunek> ...

Warunek jest jednym z następujących:

  f <wzorzec>           aktualny plik zgodny z wzorcem?
  F <wzorzec>           plik w drugim panelu zgodny z wzorcem?
  d <wzorzec>           aktualny katalog spełniający wzorzec?
  D <wzorzec>           katalog w drugim panelu spełniający wzorzec?
  t <typ>               aktualny pliku typu typ?
  T <typ>               plik w drugim panelu typu typ?
  ! <warunek>           zaprzeczenie warunku
```

Wzorzec jest normalnym wzorcem powłoki lub wyrażeniem,
podobnym do wzorca powłoki. Możesz zmienić globalne ustawienia
wzorców powłoki pisząc "shell_patterns=x" w pierwszej linii menu
użytkownika (x jest równe 0 lub 1).

```
Typ jest jednym lub więcej z podanych znaków:

  n	nie katalog
  r	zwykły plik
  d	katalog
  l	dowiązanie
  c	specjalny znak
  b	specjalny blok
  f	fifo
  s	gniazdo
  x	wykonywalny
  t	zaznaczony
```

Na przykład 'rlf' oznacza zwykły plik, dowiązanie lub fifo. Typ 't' jest
trochę odmienny ponieważ dotyczy panelu a nie pliku. Warunek '=t t' jest
prawdziwy jeśli są jakieś zaznaczone pliki w aktywnym panelu, a fałszywy jeśli
nie ma.

Jeśli warunek rozpoczyna się od '=?' zamiast '=' droga przechodzenia
przez warunki będzie pokazywana
za każdym razem kiedy warunek będzie obliczany [przydatne do wyszukiwania błędów
\- przyp. tłumacza].

Warunki są obliczane od lewej do prawej. Oznacza to, że

```
	= f *.tar.gz | f *.tgz & t n
```

jest liczone tak samo jak

```
	( (f *.tar.gz) | (f *.tgz) ) & (t n)
```

To jest prosty przykład zastosowania tych warunków:

```
= f *.tar.gz | f *.tgz & t n
L	Listuje zawartość skompresowanego archiwum tar
	gzip -cd %f | tar xvf -
```

**Warunki dodania**

Jeśli warunek rozpoczyna się od znaku '+' (lub '+?') zamiast od '=' (lub '=?')
jest to warunek dodania. Jeśli warunek jest prawdziwy, opcja menu będzie
dołączona do menu. Jeśli nie jest prawdziwy, nie będzie ona w nim zawarty.

Możesz łączyć ze sobą standardowe i dodane warunki zaczynając warunek od
kombinacji
'+=' lub '=+' (lub '+=?' lub '=+?' jeśli chcesz zobaczyć trasę błędów).
Jeśli chcesz użyć różnych warunków, dodanego i standardowego,
możesz poprzedzić wpis menu dwoma wierszami warunkowymi. Jednym zaczynającym
się od znaku '+', a drugim od '='.

Wszelkie komentarze rozpoczynają się od znaku '#'.

## Menu opcji (Options Menu) <a id="options-menu"></a>

M-Commander ma niektóre opcje, które mogą być włączane lyb wyłączane
w różnych oknach dialogowych z tego menu. Opcja jest włączona jeśli widnieje
przed nią gwiazdka lyb "x".

Komenda
*Configuration*
włącza okno dialogowe, z którego możesz zmienić
większość ustawień M-Commandera.

Menu
*Layout*
pozwala na zmianę wielu ustawień, które mają znaczący wpływ
na to jak M-Commander będzie wyglądał na ekranie.

Menu
*Confirmation*
włącza okno dialogowe, w którym możesz ustawić przy wykonaniu
których operacji chcesz być pytany o potwierdzenie.

Menu
*Learn Keys*
pokazuje okno dialogowe, w którym możesz poznać
które klawisze nie działają i w razie problemów naprawić to.

Menu
*Virtual FS*
pokazuje okno, w którym możesz zmienić niektóre ustawienia
dotyczące systemów VFS.

Komenda
*Save Setup*
zachowuje wszystkie ustawienia z menu Lewego, Prawego i Opcji.

### Konfiguracja <a id="configuration"></a>

Opcje w tym oknie są podzielone na trzy grupy:
opcje panelu (Panel Options), zatrzymaj po uruchomieniu (Pause after run) i
inne opcje (Other Options).

**Opcje panelu**

*Show Backup Files.*
Standardowo M-Commander nie wyświetla plików kończących się znakiem
'~' (tak jak komenda ls -B w wersji GNU).

*Show Hidden Files.*
Standardowo M-Commander wyświetla wszystkie pliki zaczynające się
od kropki (tak jak ls -a).

*Mark moves down.*
Standardowo kiedy zaznaczasz plik (zarówno przy klawisze Insert)
linia wyboru przenosi się o jedno w dół.

*Drop down menus.*
Kiedy ta opcja jest włączona, kiedy naciskasz klawisz
**F9**
menu będzie aktywowane, w przeciwnym wypadku zostaniesz tylko przeniosiony
do tytułów w tym menu i będziesz musiał wybrać opcję ręcznie przy użyciu
strzałek bądź też przy użyciu pierwszej litery z nazwy konkretnego menu.

*Mix all files.*
Jeśli ta opcja jest włączona, wszystkie pliki i katalogi są pomieszane razem.
Jeśli zaś jest wyłączona, katalogi (i dowiązania do nich), są listowane na
początku a pozostałe pliki dopiero za nimi.

*Fast directory reload.*
Standardowo ta opcja jest wyłączona. Jeśli ją włączysz M-Commander
będzie używał triku do sprawdzenia czy zawartość katalogu się zmieniła.
Trik polega na tym, że sprawdza się i-węzeł katalogu i jeśli się on zmienił
to katalog jest ładowany na nowo. Oznacza to przeładowywanie zawartości panelu
tylko wtedy, kiedy tworzysz lub kasujesz pliki. Jeśli robisz inne zmiany
(rozmiaru, właściciela, uprawnień, grupy itp.) będziesz musiał ręcznie przeładować
widok (np. używając kombinacji klawiszy C-r).

**Zatrzymaj po uruchomieniu**

Po wykonaniu komendy, M-Commander może zrobić pauzę, po to abyś
mógł spokojnie przejrzeć wyjście ostatniej komendy. Są trzy możliwe wartości
dla tej zmiennej:

> *Nigdy (Never)*
> Oznacza, że nie chcesz widzieć wyjścia twojej komendy. Jeśli używasz
> termianala typu Linux lub FreeBSD czy też xterm, będziesz mógł jednak
> zobaczyć jej wyjście naciskając C-o.

> *On dumb terminals*
> Będziesz miał pauzę po uruchomieniu na terminalach, które nie są w stanie
> pokazywać widoku ostatniej komendy (na wszystkich terminalach, które nie są
> xtermami lub Linux).

> *Zawsze (Always)*
> Program zatrzyma się po wykonaniu każdej z twoich komend.

**Inne opcje**

*Operacje weryfikacji (Verbose operation).*
Przełącza czy podczas kopiowania, kasowania, przenoszenia plików ma być
pokazywane okno dialogowe pokazujące stopień zaawansowania. Jeśli masz powolny
terminal, możesz chcieć wyłączyć weryfikację. Jest to wykonywane automatycznie
za ciebie jeśli twój terminal jest wolniejszy niż 9600 bps.

*Zliczaj wszystko (Compute totals).*
Jeśli ta opcja jest włączona, M-Commander zlicza wszytkie bajty
plików, które są przeznaczone do kopiowania, przenoszenia, kasowania. Spowoduje
to wyświetlanie dużo bardziej zaawansowanego wskaźnika postępu w zamian
zmiejszając trochę prędkość. Ta opcja nie ma żadnego znaczenia jeśli opcja
*Verbose operation*
jest wyłączona.

*Wzorce powłoki (Shell patterns).*
Standardowo komendy zaznacz (Select), odznacz (Unselect), i filtruj (Filter)
będą używać wyrażeń takich samych jak powłoka. Oznacza to, że
gwiazdka oznacza zero lub więcej znaków, znak zapytania dokładnie jeden znak,
a każdy inny znak sam siebie. Jeśli ta opcja jest wyłączona, stosowane są
te, których używa w komenda
**ed**(1).

*Auto Save Setup.*
Jeśli ta opcja jest włączona, kiedy wychodzisz z M-Commandera
konfiguracja M-Commander zostanie zachowana automatycznie (bez pytania)
do pliku ~/.config/mc6/ini.

*Auto menus.*
Jeśli ta opcja jest włączona, menu użytkownika będzie włączone na starcie.
Użyteczne do budowania menu dla nie unixowców.

*Używaj wewnętrznego edytora (Use internal editor).*
Jeśli ta opcja jest włączona, do edycji plików używany jest wbudowany
edytor plików. Jeśli ta  opcja jest wyłączona, używany będzie edytor wybrany
w zmiennej
**EDITOR**.
Jeśli żaden edytor nie został wybrany, używany będzie
**vi**(1).
Zobacz sekcję Wewnętrzny edytor plików.

*Używaj wewnętrznego podglądu (Use internal viewer).*
Jeśli ta opcja jest włączona, wbudowany podgląd pliku jest używany do oglądania
pliku. Jeśli ta opcja jest wyłączona, używany jest podgląd wybrany w zmiennej
**PAGER**.
Jeśli żaden podgląd nie został wybrany, wywoływana jest komenda
**view**.
Zobacz sekcję Wbudowany podgląd plików.

*Dokańczanie: pokaż wszystkie (Complete: show all).*
Standardowo M-Commander pokazuje wszystkie możliwe dokończenia
jeśli jest ich więcej, kiedy naciśniesz drugi raz klawisz
**M-Tab**,
za pierwszym razem, po prostu dokańcza to na ile można i wydaje krótki
dźwięk. Jeśli chcesz widzieć wszystkie możliwości po pierwszym naciśnięciu
**M-Tab**
włącz tę opcję.

*Obrotowy myślnik (Rotating dash).*
Jeśli ta opcja jest włączona, M-Commander będzie pokazywał obracający
się myślnik w lewym górnym rogu, jeśli będzie akurat w trakcie wykonywania
jakiegoś procesu.

*Lynx-like motion.*
Jeśli ta opcja jest włączona, możesz używać strzałek przemieszczenia
żeby automatycznie zmieniać katalog jeśli aktualnie wybrany katalog jest
podkatalogiem, a linia poleceń jest pusta. Standardowo ta opcja jest wyłączona.

*Dowiązania podążające cd (Cd follows links).*
Ta opcja, jeśli jest włączona, zmusza M-Commandera żeby podążał
za łańcuchem katalogów przy zmienianiu go w panelu czy za pomocą komendy cd.
To jest standardowe zachowanie basha. Jeśli jest wyłączona, M-Commander
podąża za prawdziwą strukturą katalogów, więc cd .. jeśli wszedłeś do
katalogu poprzez dowiązanie, przeniesie cię do prawdziwego katalogu na dysku, a nie
tam gdzie wskazywało dowiązanie.

*Bezpieczne kasowanie (Safe delete).*
Jeśli ta opcja jest włączona, nieumyślne kasowanie plików stanie się
dużo trudniejsze. Standardowy wybór w linii potwierdzenia zmienia się z
"Yes" na "No". Standardowo ta opcja jest wyłączona.

### Wygląd (Layout) <a id="layout"></a>

Meny wygląd pozwala ci na różne warianty zmieniania ogólnego wyglądu
zewnętrznego ekranu. Możesz wybrać, czy linia menu, linia poleceń, linia
hintów (pomocy) i linia klawiszy funkcyjnych mają być widoczne. Na
konsolach typu Linux lub FreeBSD możesz wybrać ile linii ma być
pokazywanych na wyjściu okna.

Reszta powierzchni ekranu jest używana przez dwa panele katalogowe. Możesz
wybrać nawet czy panele mają być ułożone poziomo czy pionowo.
Kolejną możliwością jest zmiana ich standardowej szerokości (bądź wysokości).
Jest ona standardowo równa, ale można to zmienić.

Standardowo cała zawartość panelu katalogowego jest wyświetlana tą samą barwą,
ale możesz zmienić to tak aby
*uprawnienia*
i
*typy plików*
były wyświetlane specjalnym podświetlonym kolorem.
Jeśli podświetlanie uprawnień jest włączone, część pól (ta z
*uprawnieniami*
i
*typami plików)*
będzie podświetlona przy użyciu koloru wybranego jako
*selected.*
Jeśli podświetlanie jest włączone, pliki są kolorowane w zależnośći od swojego
typu (np. katalogi, pliki typu core, wykonywalne, ...).

Jeśli opcja
*Show Mini-Status*
jest włączona, jeden wiersz informacji statusowych na temat aktualnie
wybranej rzeczy w panelu, będzie pokazany na dole panelu.

### Potwierdzanie (Confirmation) <a id="confirmation"></a>

W tym menu możesz skonfigurować opcje potwierdzania dla kasowania,
zastępowania, wykonywania przez naciśnięcie klawisza Enter, jak również
wychodzenia z programu.

### Nauka klawiszy (Learn keys) <a id="learn-keys"></a>

W tym oknie możesz przetestować czy twoje klawisz F1-F20, Home, End itp.
pracują poprawnie na twoim terminalu. Często nie działają tak, ponieważ
bazy danych terminali są poniszczone.

Przemieszczać się możesz za pomocą klawisza Tab, za pomocą klawiszy ruchu
edytora vi ('h' lewo, 'j' dół, 'k' góra i 'l' prawo) i po tym jak już raz
naciśniesz daną strzałkę (zaznaczy się ona na OK), za ich pomocą również.

Klawisze testujesz po prostu naciskając każdy z nich. Jak tylko naciśniesz
klawisz i pracuje on zupełnie poprawnie, obok nazwy klawisza powinno pojawić
się OK. Kiedy klawisz jest już sprawdzony, zaczyna pracować normalnie (np. F1
wciśnięty po raz pierwszy po prostu pokaże, że ten klawisz działa, ale
naciśnięty po raz drugi pokaże pomoc). Taka sama sytuacja powtarza się przy strzałkach.
Klawisz Tab powinien pracować zawsze.

Jeśli niektóre klawisze nie pracują poprawnie, nie zobaczysz OK obok ich nazwy
po naciśnięciu ich. Możesz chcieć je naprawić. Robisz to najeżdżając na
odpowiedni przycisk dla tego klawisza i naciskając Enter. Pokaże się wtedy
czerwona wiadomość i zostaniesz poproszony o podanie odpowiedniego klawisza.
Jeśli chcesz zrezygnować, po prostu naciśnij Esc i poczekaj do czasu kiedy
wiadomość zniknie. W przeciwnym wypadku wciśnij klawisz, który sobie życzysz
i również poczekaj na zniknięcie okna.

Kiedy skończysz już ze wszystkimi klawiszami, możesz nacisnąć Save
żeby zachować zmiany do pliku ~/.config/mc6/ini do sekcji \[terminal:TERM\] (gdzie
TERM jest nazwą twojego aktualnego terminala) lub po prostu odrzucić je.

### Wirtualny system plików (Virtual FS) <a id="virtual-fs"></a>

Ta opcja daje ci kontrolę nad ustawieniami informacji wirtualnego systemu
plików.
M-Commander zachowuje w pamięci informacje związane z niektórymi
wirtualnymi systemami plików, po to żeby kolejne połączenia przebiegały dużo
szybciej (np. ściągane listy katalogów z serwerów ftp).

Niemniej jednak, żeby mieć dostęp do zawartości skompresowanego
pliku (np. skompresowanego pliku tar) M-Commander musi
stworzyć tymczasowy nieskompresowany plik na twoim dysku.

Dopiero kiedy informacje w pamięci i tymczasowe pliki na dysku są zgodne z
zasobami, możesz chcieć zmienić parametry informacji znajdujących się w
buforze podręcznym po to, żeby zmniejszyć obciążenie dysku do mninimum albo do
zmaksymalizowania prędkości dostępu do najczęściej używanych systemów
plików.

System plików tar jest całkiem inteligentny jeśli chodzi o przechowywanie
plików: po prostu ściąga wejścia do katalogów i kiedy chcemy więcej
szczegółów o nim to system je dla nas ściąga.

W rzeczywistości jednak, pliki tar najczęściej trzymane są jako
skompresowane i jako iż natura tych plików nie pozwala na oglądanie ich bez
dekompresji (nie ma tam
widocznych od razu wejść do katalogów), system plików musi być najpierw
zdekompresowany na dysk do pliku tymczasowego i dopiero potem M-Commander ma do niego
dostęp taki jak do normalnego pliku typu tar.

Teraz, kiedy tak kochamy odwiedzać różne pliki i zwiedzać systemy
plików typu tar na całym dysku, jest całkiem prawdopodobne, że wyjdziesz
z takiego pliku, a po krótkim czasie będziesz chciał wejdść
do niego spowrotem.
Ponieważ dekompresja jest powolna, M-Commander będzie robił
kopie plików w pamięci na określony czas, po upływie którego pliki
zostaną skasowane a miejsce zajmowane przez nie zwolnione. Standardowo ten
czas ustawiony jest na jedną minutę.

System plików FTP trzyma listę katalogów z odwiedzanego przez nas
serwera w buforze podręcznym. Jego ważność konfigurowana jest za pomocą opcji
*ftpfsdirectorycachetimeout.*
Mała wartość dla tej opcji może spowolnić wszystkie operacje na systemach
ftp ponieważ każda operacja będzie wymagać kolejnych zapytań do serwera.

Ponadto możesz zdefiniować serwer proxy dla transferów ftp i skonfigurować
M-Commandera tak, aby zawsze go używał. Zobacz sekcję
System plików FTP (FTP File System) po więcej szczegółów.

### Zapisz ustawienia (Save Setup) <a id="save-setup"></a>

Na starcie M-Commander będzie próbował odczytać opcje startowe
z pliku ~/.config/mc6/ini. Jeśli on nie istnieje, odczyta on konfiguracje z
ogólnodostępnego pliku {{pkgdatadir}}/mc.ini. Jeśli on też nie istnieje M-Commander
użyje swoich domyślnych ustawień.

Komenda
*Save Setup*
tworzy plik ~/.config/mc6/ini zachowując aktualne ustawienia lewego, prawego menu,
jak również menu opcji.

Jeśli właczysz opcję
*auto save setup,*
M-Commander zawsze będzie zachowywał standardowe ustawienie podczas wychodzenia.

Istnieją również ustawienia, które nie mogą być zmienione z poziomu menu.
Dla tych ustawień musisz wyedytować swój plik konfiguracyjny za pomocą
twojego ulubionego edytora. Zobacz sekcję Specjalne ustawienia po więcej
informacji.

<!-- help:break -->

# Wykonywanie poleceń systemu operacyjnego (Executing operating system commands) <a id="executing-operating-system-commands"></a>

Możesz wykonywać komendy wpisując je bezpośrednio do linii poleceń
M-Commandera, lub wybierając program, który chcesz wykonać za pomocą klawiszy
przemieszczenia i nacisnąć Enter.

Jeśli naciśniesz Enter na pliku, który nie jest wykonywalny, M-Commander
sprawdzi rozszerzenie pliku i porówna je z rozszerzeniami wybranymi w pliku
rozszerzeń (Extensions File). Jeśli jakaś pozycja się zgadza, wykonywana
jest komenda (raczej bardziej rozszerzone makro) powiązana z tym rozszerzeniem.

## Wbudowana komenda cd (The cd internal command) <a id="the-cd-internal-command"></a>

Komenda cd jest interpretowana przez M-Commandera, nie
dokładnie tak samo jak wykonuje to powłoka. Przez to rozkaz cd nie może zawierać
wielu składników makr, które są standardowo dostępne, jednak niektórych
potrafi używać:

*Tylda*
Znak tyldy (~) jest zawsze równoznaczny z wpisaniem nazwy katalogu domowego.
Jeśli po znaku tyldy dodasz jakiś login użytkownika, zostanie on zastąpiony
przez katalog domowy wybranego użytkownika.

Na przykład, ~guest jest katalogiem domowym użytkownika guest, podczas
kiedy ~/guest jest katalogiem guest w twoim katalogu domowym.

*Poprzedni katalog (Previous directory)*
Możesz przeskakiwać do katalogu, w którym byłeś poprzednio, używając specjalnej
nazwy katalogu '-' tak jak:
**cd -**

*katalogi CDPATH*
Jeśli katalog wybrany do przejścia nie jest w naszym aktualnym katalogu, to
M-Commander używa ścieżki w zmiennej
**CDPATH**
do szukania w jakimkolwiek z wymienionych tam katalogów.

Na przykład, możesz ustawić swoją zmienną
**CDPATH**
na katalogi ~/src:/usr/src, pozwalając na zmianę katalogów na jakikolwiek
inny wewnątrz ~/src i /usr/src, z miejsca w którym jesteś (np. cd linux
przeniesie cię do katalogu /usr/src/linux).

## Obsługa makr (Macro Substitution) <a id="macro-substitution"></a>

Kiedy używamy menu użytkownika, wykonujemy plik o znajomym rozszerzeniu, lub
wykonujemy komendę z linii poleceń, możemy użyć kilku bardzo prostych makr.

Są to:

*%f*
: Nazwa aktualnego pliku.

*%d*
: Nazwa aktulnego katalogu.

*%F*
: Nazwa pliku w niewybranym panelu.

*%D*
: Nazwa katalogu w niewybranym panelu.

*%t*
: Aktualnie zaznaczone pliki.

*%T*
: Pliki zaznaczone w nieaktywnym panelu.

*%u*
i
*%U*

> Podobne w działaniu do %t i do %T jednak z tą różnicą, że pliki po ich
> użyciu zostaną odznaczone. Oznacza to, że można ich użyć tylko raz w jednym
> menu, ponieważ potem nie będzie już żadnych plików zaznaczonych.

*%s*
i
*%S*

> Wybiera: zaznaczone pliki jeśli są jakieś, w przeciwnym razie aktualny
> plik.

*%cd*
: To jest specjalne makro, które jest używane do zmieniania aktualnego katalogu
na wybrany katalog, na którego froncie jesteśmy. Jest to używane przede
wszystkim jako interfejs do wirtualnych systemów plików.

*%view*
: To makro jest używane żeby włączać wbudowany podgląd plików. Może być
ono pojedynczo lub z grupą argumentów. Jeśli postanawiasz używać któregokolwiek
z tych argumentów musisz je koniecznie wziąć w nawiasy.

> Argumentami są:
> *ascii*
> aby wymusić podgląd w trybie ascii;
> *hex*
> aby wymusić podgląd w trybie szesnastkowym;
> *nroff*
> przekazuje podglądowi, że powinien interpretować pogrubione
> i podkreślone sekwencje programu nroff;
> *unformated*
> aby przekazać podglądowi, żeby nie interpretował komend nroff aby zrobić
> tekst pogrubiony lub podkreślony.

*%%*
: Znak %

*%{jakiś tekst}*
: Pyta się o zmienną. Pokazuje się okienko wejściowe i tekst wewnątrz klamerek
używany jest jako zachęta (prompt). Makro jest zastępowane tekstem wpisanym
przez użytkownika. Użytkownik może nacisnąć ESC lub F10 aby anulować. To
makro nie działa jeszcze w linii poleceń.

## Obsługa podpowłoki (The subshell support) <a id="the-terminal"></a>

Podpowłoka (powłoka w tle) jest opcją, która musi być wybrana przy kompilacji,
działa ona z powłokami: bash, tcsh i zsh.

Jeśli powłoka w tle jest włączona do komplilacji, M-Commander będzie
sobie tworzył kopie twojej powłoki (tej zdefiniowanej w zmiennej
**SHELL**,
a jeśli nie ma, to będzie czerpał bezpośrednio z pliku /etc/passwd)
i odpalał pseudo terminal, zamiast wywoływać nową powłokę za każdym razem
kiedy wywołujesz komendę, komenda będzie przekazana powłoce w tle,
jak tylko ją napiszesz. To pozwala ci na zmianę wielu zmiennych, używanie
funkcji powłoki i zdefiniowanych aliasów, które są ważne dopóki nie wyjdziesz
z M-Commandera.

Jeśli używasz
**basha**
możesz wybrać startowe komendy twojej powłoki w tle w pliku ~/.local/share/mc6/bashrc,
a ustawienia klawiatury w ~/.local/share/mc6/inputrc.
Użytkownicy
**tcsh**
mogą wstawiać komendy startowe do pliku ~/.local/share/mc6/tcshrc.

Jeśli kod powłoki w tle jest użyty, możesz zawiesić aplikację w dowolnej chwili
po prostu naciskając kombinację C-o i przeskakując spowrotem do
M-Commandera, jeśli zawiesisz jakąś aplikację nie będziesz mógł używać innych
zewnętrznych komend zanim nie wyjdziesz z aplikacji, którą przerwałeś.

Extra dodatkiem do używania powłoki w tle jest to, że zachęta widoczna
w M-Commanderze jest tą samą, którą aktualnie używasz w powłoce.

Zobacz sekcję Opcje po więcej informacji na temat tego, jak możesz
kontrolować powłokę w tle.

# Chmod

Okno Chmod jest używane do zmieniania atrybutów grupy plików lub katalogów.
Może być ono wywołane kombinacją C-x c.

Okno Chmod ma dwie części -
*Uprawnienia (Permissions)*
i
*Plik (File)*

W sekcji Plik wyświetlana jest nazwa pliku lub katalogu i jego uprawnienia
w formie liczbowej jak również właściciel i grupa.

W sekcji Uprawnienia jest kilka przycisków, z których każdy odpowiada
za odpowiednie uprawnienie do pliku. Podczas zmieniania atrybutów, widzisz
jak zmienia się wartość liczbowa w oknie Plik.

Do poruszania pomiędzy okienkami (przyciskami i polami do zaznaczania) używaj
*strzałek*
lub klawisza
*tab.*
Aby zmienić pola lub wcisnąć przycisk używaj klawisza
*spacji.*
Możesz również używać "gorących liter" aby go wybrać
(są one podświetlonymi literami na przyciskach).

Aby uaktywnić wprowadzone zmiany wciśnij Enter.

Kiedy pracujesz z grupą plików, lub katalogów, możesz kliknąć na
bit, który chcesz wybrać lub wyczyścić. Kiedy już wybrałeś bity,
które chcesz zmienić, możesz wcisnąć jeden z przycisków aktywujących
*(Set marked*
lub
*Clear marked).*

I w końcu, aby wprowadzić dokładnie takie zmiany jak wybrałeś, użyj
przycisku
**[Set all]**,
który zadziała na wszystkich wybranych plikach.

**[Marked all]**
włącza tylko zaznaczone atrybuty do wybranych plików.

**[Set marked]**
włącza zaznaczone bity w atrybutach wszystkich wybranych plików.

**[Clean marked]**
czyści zaznaczone bity z atrybutów zaznaczonych plików.

**[Set]**
ustawia atrybuty jednego pliku.

**[Cancel]**
unieważnia komendę chmod.

# Chown

Komenda chown jest używana do zmiany właściela/grupy pliku. Skrótem
klawiszowym jest kombinacja C-x o.

# Zaawansowane chown (Advanced Chown) <a id="advanced-chown"></a>

Zaawansowane chown jest komendą łączącą w sobie komendy chmod i chown.
Możesz za jednym zamachem zmienić atrybuty i właściela/grupę pliku.

# Operacje na plikach (File Operations) <a id="file-operations"></a>

Kiedy kopiujesz, przenosisz lub kasujesz pliki, M-Commander pokazuje
okno opisowe operacji na pliku. Pokazuje nazwę pliku, na którym
aktualnie dokonuje się operacja. Widoczne są co najwyżej trzy linie postępu.
Pierwsza (file) mówi nam jak duża część pliku została już przekopiowana.
Druga (bytes) mówi jak duża część wszystkich zaznaczonych plików została
przekopiowana jak do tej pory. Trzecia (count) mówi jaka ilość plików
została już przekopiowana. Jeśli opcja verbose jest wyłączona, linia
file i bytes nie jest pokazywana.

Są dwa przyciski na dole okna dialogowego. Naciskając przycisk Skip
ominiemy resztę aktualnie "ruszanego" pliku. Naciskając przycisk Abort
zatrzymamy całą operację, pominiemy resztę plików.

Są trzy inne okna dialogowe, które mogą się włączyć podczas operacji
na plikach.

Okno błędów informuje nas o błędach zaistniałych podczas operacji
na pliku. Są w nim trzy możliwości wyboru. Przycisk Skip mówi żeby
pominąć wybrany plik, przycisk Abort żeby przerwać całą operacją,
a Retry aby ponowić próbę (np. kiedy usunąłeś problem korzystając
z innego terminala).

Okno zastępowania jest pokazywane kiedy próbujesz przenieść lub
przekopiować plik, a taki już w miejscu docelowym istnieje. Okno pokazuje
daty i wielkości obu plików. Naciśnij przycisk Yes aby nadpisać (zastąpić)
stary plik nowym, No aby pominąć ten plik, alL aby zastąpić wszystkie pliki,
nonE aby nigdy nie zastępować i Update aby zastąpić ale tylko wtedy kiedy
plik źródłowy jest nowszy niż docelowy. Całą operację możesz przerwać
naciskając przycisk Abort.

Okno rekursywnego kasowania jest pokazywane kiedy próbujesz skasować
katalog, który nie jest pusty. Naciśnij przycisk Yes aby skasować
katalog rekursywnie, No aby pominąć katalog, alL aby skasować wszystkie
katalogi rekursywnie i nonE aby pominąć wszystkie katalogi, które nie są
puste. Możesz przerwać całą opecją naciskając przycisk Abort. Jeśli
wybrałeś przycisk Yes lub alL będziesz zapytany o potwierdzenie. Wybierz
"yes" tylko jeśli jesteś pewien, że chcesz skasować wszystko rekursywnie.

Jeśli zaznaczyłeś pliki, i wykonujesz operacje tylko na nich, to jeśli
operacja się udała zostaną one odznaczone, te, na których operacja
nie przebiegła całkowicie pomyślnie, pozostaną zaznaczone.

# Maski kopiowania/przenoszenia (Mask Copy/Rename) <a id="mask-copyrename"></a>

Operacje przenoszenia i kopiowania pozwalają ci na tłumaczenie nazw
plików w łatwy sposób. Aby to zrobić, musisz wybrać odpowiednią maskę
źródłową i najczęściej w nazwie docelowej użyć gwiazdek.
Wszystkie pliki pasujące do maski źródłowej są kopiowane/przenoszone
w zgodzie z maską docelową. Jeśli są jakieś pliki zaznaczone, tylko one są
brane pod uwagę przy wybieraniu plików.

Są jeszcze inne opcje, które możesz ustawić:

Opcja
*Follow links*
mówi czy dowiązania i dowiązania twarde w katalogu źródłowym powinny być przenoszone
jako dowiązania czy też powinna być przegrywana ich zawartość (plik, na
który wskazują).

Opcja
*Dive into subdirs ...*
mówi co program ma robić, kiedy kopiuje się katalog, a taki już istnieje.
Standardowo kopiuje się pliki do wewnątrz już istniejącego katalogu (dodaje),
po włączeniu tej opcji kopiuje się katalog źródłowy do wnętrza tego katalogu.
Może przykład pomoże:

Chcesz przekopiować zawartość katalogu foo do /bla/foo, które
już istnieje. Normalnie (Dive nie jest włączone), mcommander skopiuje to dokładnie
do /bla/foo. Po włączeniu tej opcji zawartość zostanie skopiowana
do /bla/foo/foo ponieważ ten katalog już istnieje.

Opcja
*Preserve attributes*
mówi czy zachowywać oryginalne atrybuty pliku, czasy i jeśli jesteś
rootem to nawet numery UID i GID. Jeśli ta opcja jest wyłączona
używana jest aktualna wartość zmiennej umask.

**Use shell patterns on**

Jeśli opcja obsługi wzorców powłoki jest włączona, możesz używać znaków '\*' i
'?' w maskach źródłowych. Działają one tak jak w powłoce. W masce docelowej możesz
używać tylko '\*' i '\\\<cyfra>'. Pierwsza maska '\*' w nazwie docelowej
odnosi sie do pierwszej gwiazdki w masce źródłowej, druga do drugiej itd.
Joker '\\1' odnosi się do pierwszego jokera w masce źródłowej, '\\2' odnosi
się do drugiego i tak dalej aż do '\\9'. Joker '\\0' oznacza pełną nazwę
pliku źródłowego.

Dwa przykłady:

Jeśli maska źródłowa jest "\*.tar.gz", a miejscem docelowym jest "/bla/\*.tgz"
i plikiem, który ma zostać przekopiowany jest "foo.tar.gz", to kopią będzie
"foo.tgz" w katalogu "/bla".

Załóżmy, że chcesz zaminieć miejscami nazwę i rozszerzenie pliku, tak, że
plik "plik.c" ma być zmieniony na "c.plik" itp. Maska źródłowa powinna być
następująca: "\*.\*", natomiast docelowa: "\\2.\\1".

**Use shell patterns off**

Kiedy wzorce powłoki są wyłączone, M-Commander nie dokonuje automatycznego grupowania
plików. Musisz użyć wyrażenia'\\(...\\)' w masce źródłowej aby zasygnalizować
istnienie jokerów w masce docelowej. Jest to trochę łatwiejsze, ale też
wymaga aby trochę się napisać. Z drugiej jednak strony, makra są bardzo
podobne tych używanych kiedy wzorce powłoki są włączone.

Dwa przykłady:

Jeśli maską źródłową jest "^\\(.\*\\)\\.tar\\.gz$", docelową jest
"/bla/\*.tgz"
i plikiem do przekopiowania jest "foo.tar.gz", kopią będzie "/bla/foo.tgz".

Załóżmy, że chemy zamienić miejscami nazwę i rozszerzenia, tak, że plik
"plik.c" będzie się nazywał "c.plik" itp. Maską źródłową powinno być
"^\\(.\*\\)\\.\\(.\*\\)$", a docelową "\\2.\\1".

**Konwersje nazwy (Case Conversions)**

Możesz również zmieniać nazwy plików. Jeśli użyjesz '\\u' lub
'\\l' w masce docelowej, następny znak będzie przekonwertowany na
duży lub mały, zależnie od podanej opcji.

Jeśli użyjesz '\\U' lub '\\L' w masce docelowej, następne znaki będą
zmieniane na małe lub duże (zależnie od opcji), aż do napotkania znaku
'\\E' lub następnych '\\U', '\\L' bądź też końca linii.

Konwersje '\\u' i '\\l' mają wyższy priorytet niż '\\U' i '\\L'.

Na przykład, jeśli maską źródłową jest '\*' (shell patterns on) lub '^\\(.\*\\)$'
(shell patterns off) i maską docelową jest '\\L\\u\*', nazwa pliku będzie
miała pierwszą literę dużą, ale pozostałe już małe, niezależnie od obecnej
nazwy.

Możesz również używać '\\' aby "podkreślić" znak. Na przykład, '\\\\' jest
backsleshem, a '\\\*' jest gwiazdką.

# Wbudowany podgląd plików <a id="internal-file-viewer"></a>

Wbudowany podgląd plików pozwala na dwa tryby wyśmietlania: ASCII i hex.
Aby przełączać się pomiędzy tymi trybami używaj klawisza F4. Jeśli masz
zainstalowany program GNU gzip, będzie on automatycznie używany do dekompresji
plików w przypadku wystąpienia takiej potrzeby.

Podgląd plików będzie próbował użyć najlepszej metody zalecanej przez system
lub rozszerzenie pliku. Wbudowany podgląd plików będzie interpretował wiele
ciągów znaków, i włączał podkreślenie lub pogrubienie, powodując tym samym
dużo przyjemniejszy wygląd plików.

Kiedy jesteś w trybie hex, funkcja szukania akceptuje tekst w cudzysłowach
równie dobrze jak wartości szesnastkowe.

Możesz mieszać ciągi znaków ze stałymi tak jak: "Ciąg" 0xFE 0xBB
"więcej tekstu". Ciąg pomiędzy stałymi i cudzysłowami jest po prostu
ignorowany.

Tu jest lista akcji powiązanych z każdym klawiszem, który M-Commander
obsługuje w wewnętrznym poglądzie.

**F1**
Wywołuje wbudowaną przeglądarkę pomocy.

**F2**
Przełącza tryb zawijania.

**F4**
Przełącza tryb wyświetlania.

**F5**
Idź do linii. Zostaniesz zapytany o numer linii i zostanie ona wyświetlona na
ekranie twojego monitora.

**F6**, **/**.
Szukaj wyrażeń w dalszej części.

**?,**
Wsteczne wyszukiwanie wyrażenia.

**F7**
Normalne wyszukiwaniewyszukiwanie w trybie hex.

**C-s**.
Zaczyna normalne szukanie jeśli nie było żadnego wcześniej, w przeciwnym
razie szuka następnego wystąpienia.

**C-r**.
Zaczyna szukanie wsteczne jeśli jeszcze żadnego nie było, w przeciwnym
razie szuka następnego wystąpienia.

**n**.
Szuka następnego wystąpienia.

**F8**
Przełącza tryby Raw i Parsed. Pokaże to plik w postaci takiej w jakiej
został znaleziony na dysku, lub jeśli został wybrany jakiś filtr, bądź
też plik spełnia wymagania w pliku extensions.ini, wyświetlane jest to co
przekazuje filtr. Aktualne ustawienie jest zawsze przeciwne niż to napisane
na przycisku, przycisk wskazuje zawsze to co się stanie po jego
naciśnięciu.

**F9**
Przełącza pomiędzy trybami format i unformat. Kiedy tryb formatu jest
włączony podgląd będzie interpretował niektóre sentencje i pokazywał
tekst pogrubiony i podkreślony innymi kolorami. Wynika z tego, że przycisk
wskazuje co innego niż jest aktualnie (patrz wyżej).

**F10**, **Esc**.
Wychodzi z wbudowanego podglądu.

**Page Down**, **space**, **C-v**.
Przewija jedną stronę naprzód.

**Page Up**, **M-v**, **C-b**, **backspace**.
Przewija jedną stronę wstecz.

**strzałka w dół**.
Przewija jedną linię naprzód.

**strzałka w górę**.
Przewija jedną linię wstecz.

**C-l**.
Odświeża ekran.

**C-f**.
Przeskakuje do następnego pliku.

**C-b**.
Przeskakuje do poprzedniego pliku.

**M-r**.
Przełącza linijkę.

Możliwe jest poinstruowanie podglądu pliku jak ma wyświetlać plik, zobacz
sekcję Edycja pliku rozszerzeń.

# Wbudowany edytor plików <a id="internal-file-editor"></a>

Wbudowany edytor plików ma większość funkcji posiadanych przez inne
edytory pełno-ekranowe. Jest wywoływany po naciśnięciu klawisza
**F4**
o ile opcja
*use_internal_edit*
jest ustawiona w pliku startowyn. Ma maksymalny rozmiar pliku wynoszący
szesnaście megabajtów i potrafi bez skazy edytować pliki binarne.

Opcje, które aktualnie posiada to: kopiowanie, przenoszenie, kasowanie,
wycinanie i wklejanie bloków;
*klawisz dla klawisza undo;*
rozciągane menu; wklejanie plików; definiowanie makr; szukanie i
zastępowanie wyrażeń regularnych; strzałki z Shiftem zaznaczające teksty
w stylu MSW-MAC (tylko dla konsoli typu Linux); przełączanie trybu
wstawiania-zastępowania; opcja pozwalająca na "przerzucenie" bloku tekstu
przez komendę powłoki jak na przykład indent.

Edytor jest bardzo prosty w użyciu i nie wymaga żadnego przygotowania. Aby
zobaczyć jakie są klawisze po prostu obejrzyj odpowiednie menu
rozwijalne. Inne klawisze to: przemieszczanie z Shiftem zaznaczające tekst.
**Ctrl-Ins**
kopiuje do pliku
**mcedit6.clip**
a
**Shift-Ins**
wkleja z pliku mcedit6.clip.
**Shift-Del**
Wycina do
**mcedit6.clip**,
a
**Ctrl-Del**
kasuje zaznaczony tekst. Klawisze dokończenia również dają Enter z
automatycznym wcięciem. Podświetlanie myszą również działa,
i możesz je przesłonić i spowodować normalne zaznaczanie tekstu (takie jak
obsługuje terminal) po prostu trzymając klawisz Shift.

Aby zdefiniować makro, naciśnij
**Ctrl-R**
i potem naciśnij klawisze, które chcesz aby były wykonywane. Naciśnij
ponownie
**Ctrl-R**
kiedy skończysz. Możesz również przyporządkować makro do dowolnego klawisza
jaki chcesz naciskając ten klawisz. Makro jest wykonywane kiedy naciśniesz
**Ctrl-A**
i przyporządkowany klawisz. Makro jest wykonywane również jeśli naciśniesz
klawisz Meta, Ctrl, lub Esc i wybrany klawisz, jednak tylko jeśli ten
klawisz nie jest używane przez inne funkcje. Raz zdefiniowane, makro
wędruje sobie do pliku
**~/.local/share/mc6/mcedit6/mcedit6.macros**
w twoim katalogu domowym. Możesz skasować makro kasując odpowiednią linię z
tego pliku.

**F19**
sformatuje format C jeśli jest podświetlony. Żeby to działało, stwórz
wykonywalny plik
**~/.local/share/mc6/mcedit6/edit.indent.rc**
w twoim katalogu domowym zawierający poniższe:

```
#!/bin/sh
/usr/bin/indent -kr -pcs ~/.cache/mc6/mcedit6/mcedit6.block>& /dev/null
cat /dev/null > ~/.cache/mc6/mcedit6/cooledit.error
```

# Dokańczanie <a id="completion"></a>

Pozwól M-Commanderowi pisać za ciebie.

Spróbuj użyć dokończenia na tekście przed aktualną pozycją. M-Commander próbuje
dokończyć tekst jako zmienną (jeśli tekst zaczyna się od znaku
**$**),
nazwę użytkownika (jeśli tekst zaczyna się od znaku
**~**),
nazwę hosta (jeśli tekst zaczyna się od znaku
**@**)
lub komendę (jeśli jesteś w linii komend w pozycji gdzie możesz wpisać
jakąś komendę, możliwe dokończenia będą zawierać również zarezerwowane
słowa i wbudowane komendy powłoki). Jeśli żaden z powyższych warunków nie
jest spełniony, próbuje się dokańczać nazwę pliku.

Nazwa pliku, nazwa użytkownika i hosta, pracuje we wszystkich liniach
wejścia, dokańczanie komend pracuje tylko w wybranych. Jeśli dokańczanie
jest rozbudowane (jest więcej różnych możliwości), M-Commander wyda krótki dźwięk, a
następna akcja będzie zależeć od wartości zmiennej
*Complete: show all*
w menu konfiguracja. Jeśli jest ona włączona, zostanie wyświetlona lista
wszystkich możliwych nazw. Właściwą nazwę możesz wybrać za pomocą strzałek
a potem naciskając klawisz
**Enter**
na właściwej pozycji. Możesz także nacisnąć pierwsze litery, którymi różnią
się możliwości aby odrzucić tak dużą część dokończeń jak to tylko możliwe.
Jeśli naciśniesz znowu
**M-Tab**,
pokazane zostaną tylko te pozycje, które zaczynają się od kolejnych
podanych liter. Kiedy nie maja już więcej możliwości, okno znika, ale
możesz je wcześniej schować używając klawiszy anulujących:
**Esc**,
**F10**
oraz strzałek w lewo i prawo. Jeśli
Complete: show all
jest wyłączone, okno z listą włącza się dopiero wtedy, kiedy naciskasz
**M-Tab**
po raz drugi. Za pierwszym razem M-Commander wydaje tylko krótki dźwięk.

# Wirtualny system plików (Virtual File System) <a id="virtual-file-system"></a>

M-Commander jest dostarczany z kodem pozwalający na dostęp do
systemów plików. Ten kod nazywany jest wirtualnym systemem plików. Pozwala on
M-Commanderowi manipulować plikami trzymanymi na systemach nie
Unixowych.

Aktualnie M-Commander jest wyposażony w niektóre wirtualne systemy
plików (VFS): lokalny system plików, używany do dostępu do typowych
systemów plików Unixowych; ftpfs używanego do manipulowania plikami na
zdalnych systemach na poprzez protokół FTP; undelfs, używany
do odzyskiwania skasowanych plików na systemach typu ext2 (standardowy
system pracy systemu Linux), fish (do manipulowania plikami poprzez
połączenia powłok takich jak rsh czy ssh) i w końcu system mcfs (system
plików M-Commandera), oparty o sieć.

Kod VFS potrafi interpretować poprawnie wszystkie nazwy ścieżek i przekazuje
je do właściwego systemu plików. Format używany dla każdego z systemów plików
jest opisany w swojej oddzielnej sekcji.

## System plików FTP (FTP File System) <a id="ftp-file-system"></a>

Ftpfs pozwala na manipulowanie plikami na zdalnych komputerach, do
normalnego użytku, możesz próbować używać panelowych komend FTP i dowiązań
(dostępnych z linii menu) lub zmienić ścieżkę bezpośrednio za pomocą zwykłej
komendy cd wyglądającej tak jak poniżej:

*ftp://[!][użytkownik[:hasło]@]komputer[:port]/[zdalny-katalog]*

Parametry
*użytkownik,*
*port*
i
*zdalny katalog*
są opcjonalne. Jeśli wybierzesz element
*użytkownik*
M-Commander spróbuje zalogować się na zdalnym komputerze jako
zadany użytkownik, w przeciwnym razie użyje twojego loginu. Opcjonalne jest
również
*hasło,*
jeśli jest obecne zostanie użyte do nawiązania połączenia. To użycie nie
jest zalecane (tak samo jak trzymanie tego w twojej hotliście,
dopóki nie ustawisz odpowiednich uprawnień, aby nikt niepowołany nie miał
do tego dostępu).

Przykłady:

```
    ftp://ftp.nuclecu.unam.mx/linux/local
    ftp://tsx-11.mit.edu/pub/linux/packages
    ftp://!behind.firewall.edu/pub
    ftp://guest@remote-host.com:40/pub
    ftp://miguel:xxx@server/pub
```

Aby połączyć się z serwerem znajdującym się za firewallem, będziesz musiał
użyc przedrostka ftp://! aby wymusić na M-Commanderze używanie
serwera proxy do transferu danch. Serwer proxy definiuje się w oknie
dialogowym wirtualnego systemu plików.

Inną możliwością jest ustawienie opcji
*Always use ftp proxy*
w oknie konfiguracyjnym wirtualnego systemu plików. Skonfiguruje
to program tak, aby zawsze
używał serwera proxy. Jeśli ta zmienna jest ustawiona, program będzie robił
dwie rzeczy: konsultował plik {{sysconfdir}}/mcommander/mc.no_proxy w celu znalezienia linii
zawierających nazwy serwerów, które są lokalne (jeśli nazwa hosta zaczyna
się od kropki, uznaje się, że jest to domena) i sprawdza czy jakieś hosty
bez kropek w nazwie są widoczne bezpośrednio.

Jeśli używasz systemu ftpfs będąc za routerem filtrującym, który nie
pozwala ci na używanie standardowej metody otwierania plików, możesz
chcieć wymusić na programie używanie trybu passive-open. Aby tego używać
ustaw opcję ftpfs_use_passive_connections w pliku inicjującym.

M-Commander przechowuje listę katalogów w buforze podręcznym. Czas wyrzucania
bufora jest ustawiany w oknie dialogowym Wirtualnego Systemu Plików. To ma
śmieszną właściwość taką, że nawet kiedy wystąpią jakieś zmiany w katalogu, nie
będą one pokazane w strukturze katalogów, dopóki nie wymusisz tego przy
użyciu kombinacji C-r. To jest dobre rozwiązanie (jeśli myślisz, że to jest
bug, to pomyśl o pracy na zdalnych systemach położonych po drugiej stronie
Atlantyku przy użyciu ftpfs :) ).

## Transfer plików pomiędzy systemami plików (FIle transfer over SHell filesystem) <a id="file-transfer-over-shell-filesystem"></a>

System plików fish jest systemem opartym na sieci, który pozwala na
manipulowanie plikami na obcej maszynie tak jakby były one lokalne. Aby
tego używać, druga strona musi również mieć ustawiony serwer fish, lub musi
mieć powłokę kompatybilną z bashem.

Aby połączyć się z obcą maszyną, musisz tylko zmienić katalog do
specjalnego katalogu, którego nazwa jest w następującym formacie:

```
sh://[użytkownik@]komputer[:opcje];/[zdalny-katalog];</em>
```

Elementy
*użytkownik,*
*opcje*
i
*zdalny katalog*
są opcjonalne. Jeśli podasz
*użytkownika*
M-Commander spróuje zalogować się na obcy komputer jako zadany
użytkownik w przeciwnym razie użyty zostanie twój login.

Jako
*opcja*
może wystąpić 'C' - włącza kompresje i 'rsh' - włącza rsh zamist ssh. Jeśli
*zdalny-katalog*
istnieje, twój aktualny katalog na zdalnym komputerze będzie ustawiony
na niego.

Przykłady:

```
    sh://onlyrsh.mx:r/linux/local
    sh://joe@want.compression.edu:C/private
    sh://joe@noncompressed.ssh.edu/private
```

## EXTernal File System

**extfs**
allows to integrate numerous features and file types into
M-Commander in an easy way, by writing scripts.

Extfs filesystems can be divided into two categories:

1\. Stand-alone filesystems, which are not associated with any existing
file.  They represent certain system-wide data as a directory tree.
You can invoke them by typing
*'cd fsname://'*
where fsname is an extfs short name (see below).  Examples of such
filesystems include audio (list audio tracks on the CD) or apt (list of
all Debian packages in the system).

For example, to list CD-Audio tracks on your CD-ROM drive, type

```
  cd audio://
```

2\. 'Archive' filesystems (like rpm, patchfs and more), which represent
contents of a file as a directory tree.  It can consist of 'real' files
compressed in an archive (urar, rpm) or virtual files, like messages
in a mailbox (mailfs) or parts of a patch (patchfs).  To access such
filesystems
*'fsname://'*
should be appended to the archive name.  Note that the archive itself
can be on another vfs.

For example, to list contents of a zip archive documents.zip type

```
  cd documents.zip/uzip://
```

In many aspects, you could treat extfs like any other directory.  For
instance, you can add it to the hotlist or change to it from directory
history.  An important limitation is that you cannot invoke shell
commands inside extfs, just like any other non-local VFS.

Common extfs scripts included with M-Commander are:

**a**
: access 'A:' DOS/Windows diskette
*(cd a://).*

**apt**
: front end to Debian's APT package management system
*(cd apt://).*

**audio**
: audio CD ripping and playing
*(cd audio://*
or
*cd device/audio://).*

**deb**
: package of Debian GNU/Linux distribution
*(cd file.deb/deb://).*

**dpkg**
: Debian GNU/Linux installed packages
*(cd deb://).*

**hp48**
: view and copy files to/from a HP48 calculator
*(cd hp48://).*

**lslR**
: browsing of lslR listings as found on many FTPs
*(cd filename/lslR://).*

**mailfs**
: mbox-style mailbox files support
*(cd mailbox/mailfs://).*

**patchfs**
: extfs to handle unified and context diffs
*(cd filename/patchfs://).*

**rpm**
: RPM package
*(cd filename/rpm://).*

**rpms**
: RPM database management
*(cd rpms://).*

**ulha, urar, uzip, uzoo, uar, uha**
: archivers
*(cd archive/xxxx://*
where xxxx is one of:
*ulha,*
*urar,*
*uzip,*
*uzoo,*
*uar,*
*uha).*

You could bind file type/extension to specified extfs as described in the
[Edit Extension File](#edit-extension-file)
section.  Here is an example entry for Debian packages:

```
  regex/\.deb$
          Open=%cd %p/deb://
```

# Polskie znaki

M-Commander bardzo dobrze radzi sobie z obsługą znaków
nieamerykańskich (160+) w tym polskich. Ważne jest aby mieć ustawione
polskie znaki na konsoli (tzn. aby powłoka je obsługiwała). Jeśli używasz
basha musisz tylko ustawić w pliku inputrc ( /etc/inputrc lub ~/.inputrc)
następujące wartości:

```
set meta-flag on
set convert-meta off
set output-meta on
```

w pliku /etc/sysconfig/i18n:

```
SYSFONT=lat2u-16
SYSFONTACM=iso02
```

natomiast w pliku /etc/sysconfig keyboard:

```
KEYTABLE=pl
```

Potem użyj poleceń
*/sbin/setsysfont*
i
*loadkeys pl.*
[Zwróć uwagę na to, że te pliki są charakterystyczne dla dystrybucji
RedHat, jeśli masz inną i wiesz jak to ustawić, to napisz do mnie, a ja to
tu dopiszę  [ patrz tłumacz na dole ;)) ]].

I gotowe - polskie literki działają również w podglądzie i wbudowanym
edytorze plików.

# Kolory <a id="colors"></a>

M-Commander próbuje sprawdzić czy twój terminal obsługuje
kolory używając bazy danych terminali. Czasami jest to zmieniane
przez różne flagi startowe, np. możesz wymusić wyświetlanie czarno-białe
lub kolorowe startując z opcją odpowiednio -b i -c.

Jeśli program jest skompilowany z menedżerem ekranu S-Lang zamiast ncurses,
sprawdzi on również wartość zmiennej
**COLORTERM**.
Jeśli jest ustawiona, ma takie samo znaczenie jak opcja -c.

Możesz wybrać terminale, które zawsze żądają wyświetlania w kolorze,
poprzez dodanie ich do pozycji
*color_terminals*
w sekcji pliku startującego. Uchroni to M-Commandera przed próbami
odkrycia typu twojego terminala. Na przykład

```
[Colors]
color_terminals=linux,xterm
```

```
color_terminals=terminal-name1,terminal-name2...
```

Program może być skompilowany zarówno z bibliotekami S-Lang jak i ncurses.
Ncurses nie obsługuje metody wymuszania wyświetlania, zawsze sprawdza w bazie danych
terminali.

# Specjalne ustawienia <a id="special-settings"></a>

Większość ustawień M-Commandera może być zmieniana z poziomu menu.
Pomimo tego jest pewna ilość ustawień, których zmiana możliwa jest jedynie
poprzez zmianę w plikach konfiguracyjnych.

Opcje mogą być ustawione w twoim pliku ~/.config/mc6/ini :

*clear_before_exec.*
: Standardowo M-Commander czyści ekran przed wykonaniem komendy.
Jeśli chciałbyś widzieć wyjście komendy na dole ekranu, wyedytuj twój plik
~/mc/ini i zmień pole clear_before_exec na 0.

*confirm_view_dir.*
: Jeśli naciskasz F3 na katalogu, normalnie M-Commander wchodzi do niego. Jeśli ta opcja
ma wartość 1, M-Commander zapyta się o potwierdzenie przed wejściem do tego
katalogu, jeśli masz zaznaczone jakieś pliki.

*drop_menus.*
: Jeśli ta opcja jest ustawiona, kiedy naciskasz klawisz F9, rozciągane menu
będzie od razu rozłożone, w przeciwnym wypadku znajdziesz się po prostu
w najwyższym wierszu ekranu traktowanym jako menu. Będziesz musiał użyć strzałek
lub pierwszych literek, aby wybrać konkretne menu.

*ftpfs_retry_seconds.*
: Wartość jest ilością sekund, przez które M-Commander będzie czekał
cierpliwie zanim rozpocznie łączenie się z serwerem ftp od nowa. Dzieje
się to wtedy kiedy serwer odmówił połączenia lub hasło jest nieprawidłowe.
Jeśli wartość wynosi zero, nie nastąpi próba ponownego połączenia z serwerem.

*ftpfs_use_passive_connections.*
: Standardowo ta opcja jest wyłączona. Powoduje ona, że ftpfs otwiera połączenia
pasywne dla transmisji danych. Jest to używane przez ludzi, którzy siedzą
za ruterami filtrującymi. Działa to tylko wtedy, kiedy nie używasz serwera
ftp proxy.

*max_dirt_limit.*
: Opisuje jak wiele odświeżeń ekranu może być maksymalnie ominięte we wbudowanym
podglądzie plików. Normalnie ta wartość jest ważna, gdyż M-Commander automatycznie
dostosowuje liczbę odświeżeń do liczby naciśniętych klawiszy. Chociaż na bardzo
wolnych komputerach lub na klawiaturach z szybkim powtarzaniem klawiszy,
duża wartość mogłaby spowodować skoki ekranu i utratę płynności.

> Wydaje się, że wartość 10 dla max_dirt_limit jest najlepszym ustawieniem
> i to jest wartość standardowa tej funkcji.

*mouse_move_pages.*
: Kontroluje czy przewijanie w panelu za pomocą myszki odbywa się strona po
stronie czy linijka po linjce.

*mouse_move_pages_viewer.*
: Tak samo jak wyżej tylko, że we wbudowanym wewnętrznym podglądzie plików.

*navigate_with_arrows.*
: Jeśli ta opcja jest włączona, możesz używać strzałek do automatycznego
przemieszczanie się pomiędzy katalogami, jeśli linia poleceń jest pusta.
(dotyczy to strzełek w bok).

*nice_rotating_dash*
: Jeśli jest włączony, M-Commander będzie pokazywał w lewym górnym
rogu obracający się myślnik kiedy będzie wykonywał jakiś proces.

*old_esc_mode*
: Standardowo M-Commander traktuje klawisz ESC jako przedrostek
(old_esc_mode=0). Jeśli włączysz tę opcję (old_esc_mode=1), to klawisz
ESC będzie przedrostkiem dla innego klawisza, ale jeśli ten nie nastąpi,
będzie on zinterpretowany jako klawisz anulowania (tak jak ESC ESC).

*only_leading_plus_minus*
: zmienia znaczenia znaków '+', '-', '\*' w linii komend (wybór, odznaczenie,
odwrócenie zaznaczenia). Standardowo działają one tylko wtedy kiedy linia
poleceń jest pusta. Jeśli coś jest w niej już napisane, znaki te są traktowane
jako normalne. Jest to przydatne gdyż najczęściej w trakcie pisania nie chcemy
zmieniać zaznaczenia. Jednak czasami ... - wystarczy przestawić tę opcję
i klawisze te będą zawsze działać.
*panel_scroll_pages*

> Jeśli ustawione (standardowo), panel będzie przewijany o połowę za każdym
> razem kiedy kursor dochodzi do dolnej lub górnej linii, w przeciwnym wypadku
> przewijanie będzie się odbywać linia po linii.

*show_output_starts_shell*
: Ta opcja pracuje jeśli nie używasz obsługi powłoki w tle. Kiedy
użyjesz kombinacji klawiszy C-o i ta opcja jest włączona, będziesz
miał nową powłokę. Jeśli nie, dowolny klawisz przywróci znów
M-Commandera (C-o działa jak podgląd).

*show_all_if_ambiguous.*
: Standardowo M-Commander pokazuje wszystkie możliwe dokończenia
jeśli jest ich więcej i naciśnięto kombinację
**M-Tab**
po raz drugi, za pierwszym razem dokończone zostanie tylko tyle ile jest to
możliwe i jeśli będzie więcej możliwości słychać będzie krótkie bipnięcie.
Jeśli chcesz widzieć wszystkie możliwe dokończenia już po pierwszym naciśnięciu
**M-Tab**,
zmień tę opcję na 1.

*highlight_mode*
Standardowo wszystkie informacje w panelach są wyświetlane tym samym
kolorem. Jeśli ta warość jest ustawiona na 1, to
*uprawnienia*
lub
*tryb*
będą wyświetlane przy użyciu podświetlonej barwy, tak aby pokazać
ustawienia dla użytkownika. Tak więc prawa do odczytu, zapisu i wykonywania
będą wyświetlane na żółto (tzn. kolorem
*selected).*
W dodatku jeśli ta zmienna jest ustawiona na 2, to całe linie są
wyświetlane w kolorze odpowiadającym ich typowi (zobacz sekcję Kolory).
Podświetlenie uprawnień również pracuje w tym trybie.

*use_file_to_guess_type*
: Jeśli ta zmienna jest ustawiona (standardowo) próbuje się dostosować
rozszerzenie pliku do tego wybranego w pliku extensions.ini.

*xtree_mode*
: Jeśli ta opcja jest włączona (standardowo tak nie jest) kiedy przeglądasz plik
w panelu drzewa, będzie on automatycznie przeładowywał drugi panel na
zawartość wybranego katalogu.

# Baza danych terminali (Terminal databases) <a id="terminal-databases"></a>

M-Commander pozwala ci na naprawienie bazy danych terminali bez
posiadania uprawnień roota. M-Commander szuka w pliku startowym
(defaults.ini położonego w katalogach z bibliotekami M-Commandera) lub w
pliku ~/.config/mc6/ini sekcji "terminal:nazwa-twojego-terminala" i potem sekcji
"terminal:general", każda linia sekcji zawiera symbol klawisza, który
chcesz zdefiniować, zaczynające się do znaku równości i definicji klawisza.
Możesz użyć kombinacji \\E aby reprezentować znak escape i ^x aby
reprezentować znak Control-x.

Możliwymi klawiszami symboli są:

```
f0 do f20     Klawisze funkcyjne f0-f20
bs            backspace
home          klawisz home
end           klawisz end
up            strzałka w górę
down          strzałka w dół
left          strzałka w lewo
right         strzałka w prawo
pgdn          klawisz page down
pgup          klawisz page up
insert        znak insert
delete        znak delete
complete      do dokańczania
```

Na przykład, aby zdefiniować klawisz insert jako Escape + [ + O + p, możesz
ustawić to pliku ini:

```
insert=\E[Op
```

Symbol klawisza
*complete*
reprezentuje sekwencję wyjścia używaną do wywoływania procesu dokańczania,
jest to wywoływane kombinacją M-tab, ale możesz zdefiniować inne klawisze
do wykonywania tych samych funkcji (na tych klawiaturach z toną fajnych i
zupełnie bezużytecznych klawiszy).

<!-- help:break -->

# PLIKI <a id="files"></a>

Program będzie pobierał wszystkie swoje informacje ze zmiennej
**MC_DATADIR**,
jeśli jest ona nie ustawiona to znowu przetwarzany jest katalog /usr.

 {{pkgdatadir}}/help/mcommander.md
: Plik pomocy dla programu.

 {{pkgdatadir}}/extensions.ini
: Standardowy plik rozszerzeń plików.

~/.config/mc6/extensions.ini
: Własny plik użytkownika, konfiguruje podgląd i edycje plików. Ma wyższy
priorytet niż plik systemowy.

 {{pkgdatadir}}/mc.ini
: Standardowy plik setupu do M-Commandera, używany tylko wówczas,
kiedy użytkownik nie ma swojego własnego pliku ~/.config/mc6/ini.

 {{pkgdatadir}}/defaults.ini
: Globalne ustawienia M-Commandera. Ustawienia w tym pliku są
uwzględniane przez wszystkie sesje M-Commandera, użyteczne do
definiowania ogólnosystemowych ustawień terminali.

~/.config/mc6/ini
: Własny setup użytkownika. Jeśli ten plik jest dostępny, jest ładowany
zamiast pliku globalnego.

 {{pkgdatadir}}/hints/hint
: Plik zawierający podpowiedzi (hints) wyświetlane przez program.

 {{pkgdatadir}}/usermenu
: Ten plik zawiera informacje o ogólnosystemowych aplikacjach w menu.

~/.config/mc6/menu
: Własny plik menu użytkownika. Jeśli ten plik jest obecny jest używany
zamiast pliku globalnego.

\~\~/.cache/mc6/tree
: Lista katalogów drzewa katalogów i podglądu drzewa. Jedna linia jest jednym
wejściem. Linie zaczynające się od ukośnika są pełnymi nazwami katalogów.
Linie zaczynające się od numeru mają tyle znaków ile poprzedni katalog.
Jeśli chcesz możesz stworzyć plik używając komendy "find / -type d
-print | sort > ~/.cache/mc6/tree". Normalnie nie ma sensu tego czynić, gdyż
M-Commander robi to sam za ciebie.

\./.usermenu
: Lokalny plik zdefiniowany przez użytkownika. Jeśli ten plik jest dostępny,
jest używany zamiast pliku w katalogu domowym i ogólnosystemowego.

To change default home directory of M-Commander, you can use
**MC_PROFILE_ROOT**
environment variable. The value of MC_PROFILE_ROOT must be an absolute path.
If MC_PROFILE_ROOT is unset or empty, HOME variable is used. If HOME is unset
or empty, M-Commander directories are get from GLib library.

# LICENCJA <!-- help:skip -->

Program jest dystrybuowany na zasadach licencji GNU General Public License
dopóki jako publikowany przez Free Software Foundation. Zobacz wbudowaną
pomoc po więcej szczegółów na temat licencji i braku gwarancji.

# DOSTĘPNOŚĆ <a id="availability"></a>

Najnowsza wersja programu jest do zdobycia na serwerze ftp.nuclecu.unam.mc w
katalogu /linux/local i w Europie na serwerze sunsite.mff.cuni.cz w katalogu
/GNU/mc i na serwerze ftp.teuto.de w katalogu /lmb/mc.

# ZOBACZ TAKŻE <a id="see-also"></a>

ed(1), gpm(1), terminfo(1), view(1), sh(1), bash(1),
tcsh(1), zsh(1).

```
Strona M-Commander w sieci World Wide Web:
	https://github.com/blue-panels/mcommander
```

# AUTORZY <a id="authors"></a>

Miguel de Icaza (miguel@roxanne.nuclecu.unam.mx), Janne Kukonlehto
(jtklehto@paju.oulu.fi), Radek Doulik (rodo@ucw.cz), Fred
Leeflang (fredl@nebula.ow.org), Dugan Porter (dugan@b011.eunet.es),
Jakub Jelinek (jj@sunsite.mff.cuni.cz), Ching Hui
(mr854307@cs.nthu.edu.tw), Andrej Borsenkow (borsenkow.msk@sni.de),
Norbert Warmuth (nwarmuth@privat.circular.de),
Mauricio Plaza (mok@roxanne.nuclecu.unam.mx), Paul Sheer
(psheer@icon.co.za) and Pavel Machek (pavel@ucw.cz) are the developers
of this package;
Alessandro Rubini (rubini@ipvvis.unipv.it) has been especially helpful
debugging and enhancing the program's mouse support, John Davis
(davis@space.mit.edu) also made his S-Lang library available to us
under the GPL and answered my questions about it, and the following
people have contributed code and many bug fixes (in alphabetical
order):

Adam Tla/lka (atlka@sunrise.pg.gda.pl),
alex@bcs.zp.ua (Alex I. Tkachenko), Antonio Palama,
DOS port (palama@posso.dm.unipi.it), Erwin van Eijk
(wabbit@corner.iaf.nl), Gerd Knorr (kraxel@cs.tu-berlin.de),
Jean-Daniel Luiset (luiset@cih.hcuge.ch), Jon Stevens
(root@dolphin.csudh.edu), Juan Francisco Grigera, Win32 port
(j-grigera@usa.net), Juan Jose Ciarlante (jjciarla@raiz.uncu.edu.ar),
Ilya Rybkin (rybkin@rouge.phys.lsu.edu), Marcelo Roccasalva
(mfroccas@raiz.uncu.edu.ar), Massimo Fontanelli (MC8737@mclink.it),
Pavel Roskin (pavel_roskin@geocities.com),
Sergey Ya. Korshunoff (seyko2@gmail.com), Thomas Pundt
(pundtt@math.uni-muenster.de), Timur Bakeyev
(timur@goff.comtat.kazan.su), Tomasz Cholewo
(tjchol01@mecca.spd.louisville.edu), Torben Fjerdingstad
(torben.fjerdingstad@uni-c.dk), Vadim Sinolitis (vvs@nsrd.npi.msu.su)
and Wim Osterholt (wim@djo.wtm.tudelft.nl).

# BŁĘDY <a id="bugs"></a>

W pliku TODO dystrybucji znajdziesz informacje na temat tego, co
pozostało jeszcze do zrobienia.

Jeśli chcesz zgłosić kłopoty z programem [błędy w nim],
zgłoś go [po angielsku] pod adresem
<https://github.com/blue-panels/mcommander/issues> .

Do zgłoszenia błędu dołącz opis problemu, versję programu, którego używasz
(wyświetla ją mcommander -V), system operacyjny, na którym pracujesz i jeśli program
się wykłada, chcielibyśmy dostać ślad stosu.

# TŁUMACZENIE

Maciej Wojciechowski    wojciech@staszic.waw.pl
